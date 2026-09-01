@{
    # ---------------------------------------------------------------------------------
    #  Msgcore: which C++ classes the flat C ABI is a projection of.
    #
    #  Read by check_api_drift.ps1. ONE pair here, because this repository has one
    #  binding layer -- Msgcore_c.h, the 282 msgcore_* functions the README calls the
    #  supported surface. The COM servers that used to sit on it moved to MsgFacade and
    #  TargetFacade, which carry their own configs of the same shape.
    #
    #  The C API is a DELIBERATE SUBSET and is expected to stay one. Nothing here says
    #  every class member should have a C twin; it says every member without one should
    #  have a recorded reason, so that "not exposed" and "nobody got round to it" stop
    #  looking identical from the outside.
    #
    #  TypeMap is written by hand rather than derived from the msgcore_<family>_ prefixes,
    #  even though those prefixes are clean enough to derive it. A derived map would grow
    #  a new entry silently the day somebody invents a family, which is the failure this
    #  check exists to make loud.
    # ---------------------------------------------------------------------------------

    AllowFile = 'tools/ci/api-drift.allow'

    Pairs = @(
        @{
            Id = 'cxx-to-c'

            # The headers Msgcore_c.cpp itself includes. That include list is the most
            # honest available definition of "the C++ API the binding is written over":
            # it is what the wrapper can see, so it is exactly what the wrapper could
            # have bound and did not.
            Upstream = @{
                Kind  = 'cxx'
                Files = @(
                    'P2PmsgMgr.h'
                    'P2Pmsg.h'
                    'P2PmsgBSTR.h'
                    'MsgAttr.h'
                    'MsgDesc.h'
                    'MsgCurs.h'
                    'MsgList.h'
                    'MsgVect.h'
                    'MsgStck.h'
                )
            }

            Surface = @{
                Kind     = 'cfn'
                Files    = @('Msgcore_c.h')
                ApiMacro = 'MSGCORE_C_API'
            }

            # class -> the msgcore_ prefix its members are expected to appear under.
            # The nine families are the ones Msgcore_c.h actually spells; a class with
            # no family at all (P3PmsgData, P3PmsgName, P3PmsgTime, P3PmsgObject) is
            # absent here on purpose -- it has no C projection by design, and saying so
            # once in this comment beats several hundred allowlist lines saying it again.
            TypeMap = @{
                'P2PmsgMgr'    = 'msgcore_mgr_'
                'P3PmsgField'  = 'msgcore_field_'
                'P3PmsgAttr'   = 'msgcore_attr_'
                'P3PmsgDesc'   = 'msgcore_desc_'
                'P3PmsgCurs'   = 'msgcore_curs_'
                'P3PmsgList'   = 'msgcore_list_'
                'P3PmsgVect'   = 'msgcore_vect_'
                'MsgStck'      = 'msgcore_stck_'
                'P2PmsgRecurs' = 'msgcore_recurs_'
                'P3PmsgBSTR'   = 'msgcore_bstrio_'
            }
        }
    )
}
