target("WeaselServer")
  set_kind("binary")
  add_files("./*.cpp")  -- includes PhrasesDialog.cpp via glob
  add_includedirs("$(projectdir)/WeaselUI", "$(projectdir)/WeaselUI/FluxingComponents")
  add_rules("add_rcfiles", "subwin")
  add_links("imm32", "kernel32", "rime", "windowscodecs")
  add_deps("WeaselUI", "WeaselIPC", "RimeWithWeasel", "WeaselIPCServer")

  add_files("$(projectdir)/PerMonitorHighDPIAware.manifest")
  -- L127: no /openmp here either -- see WeaselUI/xmake.lua. With the
  -- dynamic OpenMP runtime the binary imports VCOMP140.dll, which the
  -- installer does not ship -> on a clean machine the process cannot even
  -- start. "/openmp:static" does not help: this toolchain has no such
  -- option (cl /help knows only /openmp, /openmp:experimental and
  -- /openmp:llvm), and libomp.lib is the import library of libomp140.dll,
  -- so there is no static OpenMP runtime to link at all. See L129.
  add_ldflags("/DEBUG /LARGEADDRESSAWARE /ERRORREPORT:QUEUE /OPT:NOREF /OPT:NOICF")
  add_ldflags("/LTCG:OFF", {force = true})
  set_policy("windows.manifest.uac", "invoker")
  before_build(function(target)
    local target_dir = path.join(target:targetdir(), target:name())
    if not os.exists(target_dir) then
      os.mkdir(target_dir)
    end
    target:set("targetdir", target_dir)
  end)
  after_build(function(target)
    if is_arch("x86") then
      os.cp(path.join(target:targetdir(), "WeaselServer.exe"), "$(projectdir)/output/Win32")
      os.cp(path.join(target:targetdir(), "WeaselServer.pdb"), "$(projectdir)/output/Win32")
    else
      os.cp(path.join(target:targetdir(), "WeaselServer.exe"), "$(projectdir)/output")
      os.cp(path.join(target:targetdir(), "WeaselServer.pdb"), "$(projectdir)/output")
    end
  end)

