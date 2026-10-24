承接 `handoff-github-push-2026-08-10.md`(项目根,必读 §6 列了 5 个真相源)— Fluxing v0.21.0.0 已 push 到 GitHub remote(divergence 0/0),L108 已写 lessons-learned。branch Fluxing,HEAD 792cb47e,tag v0.21.0.0 + backup tag 全在 remote。

下步必交付(3 选 1):
(a) v0.21.0.1 ship:start_service.bat 内容反 fix(trivial)+ output/data/user-custom/*.custom.yaml key_binder send 中文 parse error fix(需研究 librime send_text API vs punctuator/half_shape patch)
(b) 并发压测验证:装 v0.21.0.0 后同时开 Claude Code + Explorer + dopus + 微信 + WeaselServer 重启,来回切 5+ min,看 C:\fluxing-dumps 是否新增 0xc0000374 dump(manual,user)
(c) 跑 sandbox-verify\rime-verify.ps1 -InstallerPath release\fluxing-0.21.0.0-installer.exe(注意 Win11 24H2 Sandbox LogonCommand flake 风险)

禁止事项:
改 output/install.nsi(F3/F4 reliability verified PASS 不能动)
重新跑 _nsis_v065.cmd rebuild v0.21.0.0 installer(只生成相同 md5)
删 release/fluxing-0.20.0.{0,1,3,4}-installer.exe(per feedback_release_output_rule)
在 D:\Program Files\Fluxing\weasel\ 跑 uninstall.exe(破坏装机 state)
直接 git push kizemo Fluxing(走 HTTPS 必 chunked,改用 kizemo-ssh remote alias + 200/批 chunked)
echo sftp.json password 到 log

加速模式规则(per CLAUDE.md):
任何 silent-failure-prone 代码 → 加 IfErrors 检测 + retry + 备用路径(L108 + PhaseM 教训)
ship 前 md5 dual-verify 是 ship-gate(extract archive == user dir)
bash tool output capture 对长 push 不可靠 → 用 git ls-remote ground truth
silent failure 最差 → 任何安装/registry/mutex 改动必带 loud log