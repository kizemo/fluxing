# Relay kit install report

- installed_at: 2026-11-27 09:00:18
- tool: relay-port skill (E:\Users\Duanyi\.minimax\.minimax\skills\relay-port)
- repo: F:\soft\00selfmade\rime_claude
- runtime: mcode
- rule_files: AGENTS.md
- target: BR-①g 桶化双写后续批次（2026-11-27 起）
- plan_doc: docs\plan-relay.md
- max_stages: 6
- verdict: PASS

## Console log

```
REPO root=F:\soft\00selfmade\rime_claude
REPO name=rime_claude
REPO head=754894d
RUNTIME name=mcode spawn_template=ACTIVATION-SPAWN-mcode.md
PLAN doc=docs\plan-relay.md created=False
SKIP relay-lock.ps1 identical bytes=6429
SKIP start-relay.ps1 identical bytes=35754
SKIP relay-ledger.ps1 identical bytes=19826
SKIP relay-spawn.ps1 identical bytes=16484
SKIP AGENT-RELAY.md identical bytes=23617
SKIP RELAY-SOP.md identical bytes=7129
ADAPT answers=docs\relay\adaptation-answers.json applied=8/8
PATCH RELAY-ADAPTATION.md sections=3,4,5 missing= answers=docs\relay\adaptation-answers.json
ADAPT_CHECK doc=docs\RELAY-ADAPTATION.md placeholders=0 source=answers
AGENTS action=present_kept reason=n/a before=[bytes=42421 sha16=C40EE34F0E099BBD] after=[bytes=42421 sha16=C40EE34F0E099BBD] bom=False
WRITE relay-runtime.json bytes=904 mode=session_tool
WRITE ACTIVATION-PROMPT.txt bytes=3053
LEDGER absent (the launcher creates it at activation; the installer never writes it)
ASCII_CHECK file=relay-lock.ps1 non_ascii=0 bom=False bytes=6429
ASCII_CHECK file=start-relay.ps1 non_ascii=0 bom=False bytes=35754
ASCII_CHECK file=relay-spawn.ps1 non_ascii=0 bom=False bytes=16484
AGENTS_MARKERS begin=1 end=1
LOCK_STATUS exit=0 line=LOCK_FREE path=F:\soft\00selfmade\rime_claude\docs\relay\.relay.lock
DRYRUN exit=0 verdict=OK artifact=F:\soft\00selfmade\rime_claude\docs\relay\install-dryrun.txt
```

## Adaptation status

- status: confirmed
- source: answers
- doc: docs\RELAY-ADAPTATION.md

## Next step

The adaptation values were confirmed from docs\relay\adaptation-answers.json.
Open a session whose workspace IS this repo and say:

    按 docs\relay\ACTIVATION-PROMPT.txt 启动接力链
