#!/bin/sh
# assumes running from top level ddcutil directory
autoreconf --force -I config -I m4 --install --verbose
test -n "$NOCONFIGURE" || ./configure "$@"

