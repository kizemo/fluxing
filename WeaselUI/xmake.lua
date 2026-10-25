target("WeaselUI")
  set_kind("static")
  add_files("./*.cpp")
  add_files("./FluxingComponents/*.cpp")
  -- L127: do NOT enable /openmp here.
  --
  -- GdiplusBlur.cpp uses "#pragma omp parallel for", so it wants OpenMP. But
  -- with /openmp the MSVC compiler links the DYNAMIC OpenMP runtime: the
  -- objects reference _vcomp_for_static_* and the final binary imports
  -- VCOMP140.dll -- a DLL the installer does not ship. On any machine
  -- without the Visual C++ redistributable the TSF shim then cannot be
  -- loaded at all: regsvr32 fails with ERROR_MOD_NOT_FOUND,
  -- DllRegisterServer never runs, and the TIP is never registered (no
  -- Fluxing in the input switcher, no candidate window).
  --
  -- "/openmp:static" is NOT a fix here. This toolchain has no such option
  -- at all: cl /help only knows /openmp, /openmp:experimental and
  -- /openmp:llvm, and passing /openmp:static is treated as plain /openmp
  -- (cl warns D9035). On top of that there is no static OpenMP runtime to
  -- link: vcomp.lib (28 KB) is the IMPORT library of VCOMP140.dll, and
  -- libomp.lib (186 KB) is the import library of libomp140.dll (LLVM ABI)
  -- -- linking it adds a NEW unshipped DLL dependency (verified with
  -- dumpbin /dependents). See lessons-learned L129.
  --
  -- The MSBuild path has OpenMP off as well: WeaselUI.vcxproj spells the
  -- property "<OpenMP>" instead of "<OpenMPSupport>", so MSBuild silently
  -- ignores it and never passes /openmp. Keeping this file the same way
  -- keeps the two build paths at parity and keeps the shipped artifact
  -- free of an unshipped runtime dependency.
  --
  -- Enabling OpenMP here would mean shipping the whole VC redistributable
  -- (~25 MB) to speed up a sub-millisecond shadow blur. Not worth it: if
  -- the blur ever gets visibly slow, cache the blurred shadow instead.
