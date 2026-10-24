# Task: 4 user-reported bug root cause (read-only)

## User 4 bug (装机 v0.19.0.34 后)
1. UserDict 文本/按键遮挡
2. UserDict 点击消失, 功能未实现
3. PhrasesDialog 输入框只能英文
4. PhrasesDialog 缺分类 + 默认选 + 回车上屏 + 上下箭头

## Plan (5 axis)
- Axis 1: UserDict 遮挡 → UserDictionary.cpp kDialogH/按钮位置
- Axis 2: UserDict 点击 → WM_COMMAND 6 按钮 handler
- Axis 3: PhrasesDialog IME → CreateWindowExW Edit control + IMM
- Axis 4: PhrasesDialog 缺分类 → TreeView + Phrase.category + 键盘 handler
- Axis 5: git history 对比 → 找老 UI commit 引用

## Status
- [ ] Axis 1-5 read
- [ ] git history 对比
- [ ] 5-axis 报告
- [ ] 修复方案排序
- [ ] 老 git 引用