# tmp_handoff

本目录用于 Claude Code 与 Codex / ChatGPT 在项目开发过程中的临时交接，不用于存放正式设计文档。

## 角色

- Claude Code：技术负责人
- Codex / ChatGPT：工程师

## 文件类型

- `plan_*.md`
  - 由 Claude Code 创建
  - 用于下发阶段目标、任务边界、约束与验收标准

- `review_*.md`
  - 由 Claude Code 创建
  - 用于输出 Review 结论、问题列表、测试结果与修改要求

- `report_*.md`
  - 由 Codex / ChatGPT 创建
  - 用于反馈实现结果、变更文件、验证情况、风险与待确认项

## 命名规则

统一格式：

`<type>_<stage>_<yyyymmdd>_<hhmmss>.md`

示例：

- `plan_main_window_20260331_140500.md`
- `review_main_window_20260331_173000.md`
- `report_main_window_20260331_181500.md`

## 使用顺序

1. Claude Code 创建当前阶段的 `plan_*.md`
2. 工程师读取该计划并完成实现
3. 工程师提交 `report_*.md`
4. Claude Code 基于 `report_*.md` 输出 `review_*.md`
5. 若 `review` 为 `changes_requested`，工程师继续修改并重新提交新的 `report_*.md`
6. 若 `review` 为 `pass`，该阶段结束

## 当前生效文件规则

- 同一阶段默认以同类型中时间戳最新的文件为准
- 若需显式替代旧文件，应在文件头中填写：
  - `Supersedes: <old_filename>`

## 模板

本目录内提供以下模板：

- `plan_template.md`
- `review_template.md`
- `report_template.md`

新建交接文件时应优先基于模板创建。

## 注意事项

- 正式结论必须落文件，不仅写在聊天消息中
- 一个交接文件只处理一个阶段的一次交接
- 若文档、协议、接口存在冲突，先暂停实现，再由 Claude Code 重新给出结论
- 正式项目规则以项目根目录下 `claude.md` 为准
