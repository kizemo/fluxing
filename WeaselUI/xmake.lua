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
  -- "/openmp:static" is NOT a fix here: it is a CL-driver option, and xmake
  -- calls link.exe directly, so the driver never swaps vcomp140.lib for
  -- libomp.lib and the import survives (verified: dumpbin /dependents still
  -- lists VCOMP140.DLL).
  --
  -- The MSBuild path has OpenMP off as well: WeaselUI.vcxproj spells the
  -- property "<OpenMP>" instead of "<OpenMPSupport>", so MSBuild silently
  -- ignores it and never passes /openmp. Keeping this file the same way
  -- keeps the two build paths at parity and keeps the shipped artifact
  -- free of an unshipped runtime dependency.
  --
  -- Turning OpenMP on properly (static libomp in BOTH build systems) is a
  -- separate task; see lessons-learned L127 and AGENTS.md A16.
