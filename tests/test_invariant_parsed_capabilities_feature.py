import pytest
import ctypes
import struct
import sys

# Simulate the parsed capabilities feature parsing logic in Python
# to test the security invariant: buffer reads never exceed declared length

CAPABILITIES_FEATURE_MARKER = b"VCPF"
MAX_VALUE_STRING_BUFFER = 256  # Expected maximum buffer size


class ParsedCapabilitiesFeature:
    """Python simulation of the C struct and parsing logic."""
    
    BUFFER_SIZE = 256  # Fixed allocation size for value_string
    
    def __init__(self):
        self.marker = bytearray(4)
        self.value_string = bytearray(self.BUFFER_SIZE)
        self.value_string_len = 0
    
    def parse_safe(self, value_string_start: bytes, value_string_len: int) -> bool:
        """
        Safe version: validates length before copy.
        Returns True if parsing succeeded, False if rejected.
        """
        if value_string_len < 0:
            return False
        if value_string_len > self.BUFFER_SIZE:
            return False
        if value_string_len > len(value_string_start):
            return False
        
        self.marker[:4] = CAPABILITIES_FEATURE_MARKER
        self.value_string[:value_string_len] = value_string_start[:value_string_len]
        self.value_string_len = value_string_len
        return True
    
    def parse_vulnerable(self, value_string_start: bytes, value_string_len: int):
        """
        Vulnerable version: copies value_string_len bytes without validation.
        This simulates the CWE-120 vulnerability.
        """
        self.marker[:4] = CAPABILITIES_FEATURE_MARKER
        # Vulnerable: no bounds check on value_string_len
        # In C this would be: memcpy(vfr->value_string, value_string_start, value_string_len)
        # Here we simulate what would happen
        actual_copy_len = min(value_string_len, len(value_string_start))
        self.value_string[:actual_copy_len] = value_string_start[:actual_copy_len]
        self.value_string_len = value_string_len


def build_ddc_response(value_string: bytes, declared_len: int) -> tuple:
    """Build a simulated DDC/CI monitor response payload."""
    return (value_string, declared_len)


# Attack payloads: (value_string_data, declared_length, description)
ATTACK_PAYLOADS = [
    # Exact boundary - should succeed
    (b"A" * 256, 256, "exact_boundary"),
    
    # 2x oversized declared length
    (b"B" * 256, 512, "2x_oversized_declared_len"),
    
    # 10x oversized declared length
    (b"C" * 256, 2560, "10x_oversized_declared_len"),
    
    # Declared length larger than actual data
    (b"D" * 10, 256, "declared_larger_than_data"),
    
    # Maximum integer-like value
    (b"E" * 256, 65535, "max_uint16_declared_len"),
    
    # Declared length = BUFFER_SIZE + 1 (off-by-one)
    (b"F" * 257, 257, "off_by_one_overflow"),
    
    # Empty data with large declared length
    (b"", 1000, "empty_data_large_declared_len"),
    
    # Null bytes in payload
    (b"\x00" * 512, 512, "null_bytes_2x_overflow"),
    
    # Mixed content oversized
    (b"\xff\xfe\xfd" * 200, 600, "mixed_content_oversized"),
    
    # Declared length = 0 (edge case)
    (b"G" * 100, 0, "zero_declared_len"),
    
    # Negative declared length (integer overflow simulation)
    (b"H" * 100, -1, "negative_declared_len"),
    
    # Very large declared length (potential integer overflow)
    (b"I" * 256, 2**31 - 1, "max_int32_declared_len"),
    
    # Declared length = BUFFER_SIZE * 10
    (b"J" * 256, 256 * 10, "10x_buffer_size"),
    
    # Crafted DDC/CI-like response with oversized section
    (b"\x6e\x6c\x00\xff" + b"K" * 300, 304, "ddc_ci_like_oversized"),
    
    # Unicode-like bytes that might confuse length calculations
    (b"\xc3\xa9" * 200, 400, "multibyte_sequence_oversized"),
]


@pytest.mark.parametrize("value_string_data,declared_len,description", ATTACK_PAYLOADS)
def test_buffer_read_never_exceeds_declared_length(value_string_data, declared_len, description):
    """
    Invariant: Buffer reads never exceed the declared length and never exceed
    the allocated buffer size. When parsing DDC/CI monitor responses (untrusted input),
    a crafted response with an oversized value_string_len must be rejected or truncated,
    never causing a heap buffer overflow (CWE-120).
    """
    feature = ParsedCapabilitiesFeature()
    
    # The safe parser must either:
    # 1. Reject the input (return False) when declared_len > BUFFER_SIZE or invalid
    # 2. Accept and copy only up to BUFFER_SIZE bytes
    result = feature.parse_safe(value_string_data, declared_len)
    
    if result:
        # If parsing succeeded, the copied length must never exceed buffer size
        assert feature.value_string_len <= ParsedCapabilitiesFeature.BUFFER_SIZE, (
            f"[{description}] value_string_len ({feature.value_string_len}) "
            f"exceeds buffer size ({ParsedCapabilitiesFeature.BUFFER_SIZE})"
        )
        
        # The actual stored length must match what was declared (if accepted)
        assert feature.value_string_len == declared_len, (
            f"[{description}] Stored length mismatch: "
            f"stored={feature.value_string_len}, declared={declared_len}"
        )
        
        # Verify no out-of-bounds: declared_len must be within buffer
        assert declared_len <= ParsedCapabilitiesFeature.BUFFER_SIZE, (
            f"[{description}] Parser accepted oversized declared_len={declared_len} "
            f"which exceeds BUFFER_SIZE={ParsedCapabilitiesFeature.BUFFER_SIZE}"
        )
        
        # Verify the data was not read beyond the source buffer
        assert declared_len <= len(value_string_data), (
            f"[{description}] Parser accepted declared_len={declared_len} "
            f"larger than source data length={len(value_string_data)}"
        )
    else:
        # If parsing was rejected, verify it was for a valid security reason
        rejection_is_valid = (
            declared_len < 0 or
            declared_len > ParsedCapabilitiesFeature.BUFFER_SIZE or
            declared_len > len(value_string_data)
        )
        assert rejection_is_valid, (
            f"[{description}] Parser rejected valid input: "
            f"declared_len={declared_len}, data_len={len(value_string_data)}, "
            f"buffer_size={ParsedCapabilitiesFeature.BUFFER_SIZE}"
        )


@pytest.mark.parametrize("value_string_data,declared_len,description", ATTACK_PAYLOADS)
def test_no_read_beyond_source_buffer(value_string_data, declared_len, description):
    """
    Invariant: The parser must never attempt to read more bytes from the source
    buffer than are actually available. Reading beyond source data is undefined
    behavior and a security vulnerability.
    """
    feature = ParsedCapabilitiesFeature()
    
    # Track actual bytes available
    available_bytes = len(value_string_data)
    
    result = feature.parse_safe(value_string_data, declared_len)
    
    if result:
        # Bytes actually copied must not exceed available source bytes
        assert feature.value_string_len <= available_bytes, (
            f"[{description}] Read {feature.value_string_len} bytes but only "
            f"{available_bytes} bytes available in source buffer"
        )


@pytest.mark.parametrize("value_string_data,declared_len,description", ATTACK_PAYLOADS)
def test_marker_integrity_after_parse(value_string_data, declared_len, description):
    """
    Invariant: The CAPABILITIES_FEATURE_MARKER must remain intact after parsing,
    indicating no buffer overflow corrupted adjacent memory regions.
    """
    feature = ParsedCapabilitiesFeature()
    
    result = feature.parse_safe(value_string_data, declared_len)
    
    if result:
        # Marker must be correctly set and not corrupted
        assert bytes(feature.marker) == CAPABILITIES_FEATURE_MARKER, (
            f"[{description}] Marker corrupted after parsing: "
            f"expected={CAPABILITIES_FEATURE_MARKER}, got={bytes(feature.marker)}"
        )


@pytest.mark.parametrize("multiplier", [1, 2, 5, 10, 100, 1000])
def test_oversized_by_multiplier(multiplier):
    """
    Invariant: Regardless of how much the declared length exceeds the buffer size,
    the parser must always reject or safely truncate the input.
    """
    buffer_size = ParsedCapabilitiesFeature.BUFFER_SIZE
    oversized_len = buffer_size * multiplier
    
    # Create data that matches the oversized declared length
    value_string_data = b"X" * min(oversized_len, 10000)  # Cap actual data to avoid OOM
    
    feature = ParsedCapabilitiesFeature()
    result = feature.parse_safe(value_string_data, oversized_len)
    
    if multiplier == 1:
        # Exact size should succeed
        assert result is True, (
            f"Parser rejected valid input at exact buffer size (multiplier={multiplier})"
        )
        assert feature.value_string_len <= buffer_size
    else:
        # Oversized should be rejected
        assert result is False, (
            f"Parser accepted oversized input: declared_len={oversized_len} "
            f"({multiplier}x buffer_size={buffer_size})"
        )


def test_sequential_oversized_attacks():
    """
    Invariant: Multiple sequential parsing attempts with oversized inputs
    must not cause cumulative buffer corruption.
    """
    feature = ParsedCapabilitiesFeature()
    
    attack_sequences = [
        (b"A" * 1000, 1000),
        (b"B" * 500, 500),
        (b"C" * 257, 257),
        (b"D" * 256, 256),  # This one should succeed
        (b"E" * 2000, 2000),
    ]
    
    for i, (data, declared_len) in enumerate(attack_sequences):
        result = feature.parse_safe(data, declared_len)
        
        # After each attempt, verify buffer integrity
        assert len(feature.value_string) == ParsedCapabilitiesFeature.BUFFER_SIZE, (
            f"Buffer size changed after attack sequence step {i}: "
            f"expected={ParsedCapabilitiesFeature.BUFFER_SIZE}, "
            f"got={len(feature.value_string)}"
        )
        
        if result:
            assert feature.value_string_len <= ParsedCapabilitiesFeature.BUFFER_SIZE, (
                f"Accepted parse at step {i} has oversized value_string_len: "
                f"{feature.value_string_len}"
            )