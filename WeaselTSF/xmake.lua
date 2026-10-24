target("WeaselTSF")
  set_kind("shared")
  add_files("./*.cpp", "WeaselTSF.def")
  add_rules("add_rcfiles", "use_weaselconstants")
  add_deps("WeaselIPC", "WeaselUI", "RimeWithWeasel")
  add_links("RimeWithWeasel")
  -- spec 063: link delayimp.lib explicitly. xmake does not auto-add
  -- it; without it, /DELAYLOAD fails with LNK4199. Also re-add the
  -- delay-load shflag (it was in the previous add_shflags line).
  add_links("delayimp")
  local fname = ''
  if is_arch("x86") then
    fname = "weasel.dll"
  elseif is_arch("x64") then
    fname = "weaselx64.dll"
  elseif is_arch("arm") then
    fname = "weaselARM.dll"
  elseif is_arch("arm64") then
    fname = "weaselARM64.dll"
  end
  set_filename(fname)

  add_files("$(projectdir)/PerMonitorHighDPIAware.manifest")
  -- spec 063: delay-load api-ms-win-shcore-scaling-l1-1-1.dll.
  -- WeaselUI calls GetDpiForMonitor() (WeaselPanel.cpp:87,187) which
  -- is imported from this API set when built against the Win 11 SDK
  -- (10.0.26100.0). On Win 10 / older OS, this DLL is not present
  -- in System32, causing regsvr32 exit 3 (ERROR_MOD_NOT_FOUND) and
  -- DllRegisterServer never runs (RegisterProfiles + RegisterCategories
  -- never fire -> KnownClasses False -> QuickPanel cannot work).
  -- The delay-load pragma is also in WeaselPanel.cpp (defensive).
  --
  -- L127: no /openmp here either -- see WeaselUI/xmake.lua for the whole
  -- story. The short version: /openmp links the dynamic OpenMP runtime, the
  -- binary ends up importing VCOMP140.dll, and the installer does not ship
  -- that DLL, so on a clean machine the TSF shim cannot be loaded at all
  -- (regsvr32 -> ERROR_MOD_NOT_FOUND -> the TIP is never registered).
  -- "/openmp:static" does not help here: it is a CL-driver option and xmake
  -- calls link.exe directly, so the driver never swaps vcomp140.lib for
  -- libomp.lib (verified with dumpbin /dependents).
  add_shflags("/DEBUG /LTCG:OFF /OPT:NOREF /OPT:NOICF /DELAYLOAD:api-ms-win-shcore-scaling-l1-1-1.dll", {force = true})
  before_build(function(target)
    local target_dir = path.join(target:targetdir(), target:name())
    if not os.exists(target_dir) then
      os.mkdir(target_dir)
    end
    target:set("targetdir", target_dir)
  end)

  after_build(function(target)
    os.cp(path.join(target:targetdir(), "weasel*.dll"), "$(projectdir)/output")
    os.cp(path.join(target:targetdir(), "weasel*.pdb"), "$(projectdir)/output")
  end)
