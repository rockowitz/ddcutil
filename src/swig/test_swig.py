import sys
# s.path.insert(0,"/usr/local/lib64/python2.7/site-packages/")
# print "sys.path:", sys.pat#
import ddc_swig
# print dir(ddc_swig)
from ddc_swig import *

def get_build_information():
   ddcutil_version = ddcs_ddcutil_version_string()
   print('ddcutil version:   %s' % ddcutil_version)
   build_flags = ddcs_get_build_options()
   print( 'build_flags: %s' % repr(build_flags))
   # for v in build_flags:
   #    print v
   # print( 'len(build_flags) = %d' % len(build_flags))
   # print( 'DDCS_HAS_ADL:  %s' % DDCS_HAS_ADL)
   # print( 'DDCS_HAS_USB:  %s' % DDCS_HAS_USB)
   # print( 'DDCS_HAS_FAILSIM: %s' % DDCS_HAS_FAILSIM)
   # n. requires parentheses around ( .. in ..)
   print( "Built with USB support:         %s" % (DDCS_HAS_USB     in build_flags))
   print( "Built with ADL support:         %s" % (DDCS_HAS_ADL     in build_flags))
   print( "Built with failure simulation:  %s" % (DDCS_HAS_FAILSIM in build_flags))




def test1():
   print( "Default output level: %d" % ddcs_get_output_level() )
   ddcs_set_output_level(DDCS_OL_VERBOSE);
   print( "Output level reset to %s" %ddcs_output_level_name(ddcs_get_output_level()) )
   
   print( "Initial report ddc errors setting: %s" % ddcs_is_report_ddc_errors_enabled() )
   ddcs_enable_report_ddc_errors(True)
 
   displayct = ddcs_report_active_displays(2)
   print( "Found %d active displays" % displayct )

   did = ddcs_create_dispno_display_identifier(2)
   print( ddcs_repr_display_identifier(did))
   dref = ddcs_get_display_ref(did)
   print( ddcs_repr_display_ref(dref) )
   ddcs_report_display_ref(dref, 2)
   dh = ddcs_open_display(dref)
   print( ddcs_repr_display_handle(dh) )

   print( "Name of feature 0x10: %s" % ddcs_get_feature_name(0x10) )
   # feature_flags = ddcs_get_feature_info_by_display(dh, 0x10)
   # print "Feature 0x10 flags: %s, %d, 0x%08x" % (feature_flags, feature_flags, feature_flags)

   vcp_val = ddcs_get_nontable_vcp_value(dh, 0x10)
   print( vcp_val )
   print( "cur value: ", vcp_val.cur_value )
   print( "max value: ", vcp_val.max_value )
   ddcs_set_nontable_vcp_value(dh, 0x10, 22)
   print( "value reset to", ddcs_get_nontable_vcp_value(dh, 0x10).cur_value )

   ddcs_set_nontable_vcp_value(dh, 0x10, vcp_val.cur_value)
   print( "value reset to", ddcs_get_nontable_vcp_value(dh, 0x10).cur_value )

   caps  = ddcs_get_capabilities_string(dh)
   print( "Capabilities: %s" % caps )

   profile_vals = ddcs_get_profile_related_values(dh)
   print( "Profile related values: %s" % profile_vals  )


   # print( vcp_val.cur_value()     # int object is not callable )
   # ddcs_close_display(dh)

if __name__ == "__main__":
   print( "(test_swig)" )
   print( dir(ddc_swig) )
   print()
   get_build_information() 
   print()
   # test1()


