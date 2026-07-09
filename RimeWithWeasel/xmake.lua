target("RimeWithWeasel")
  set_kind("static")
  add_files("./*.cpp")
  add_rules("use_weaselconstants")
  add_includedirs("$(projectdir)/WeaselServer")  -- spec 056: FocusIn/FocusOut
                                                    --   call into QuickPanelDialog.h

