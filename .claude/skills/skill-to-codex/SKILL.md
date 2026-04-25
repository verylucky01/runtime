---
name: skill-to-codex
description: 将当前项目 .claude/skills 下的 Claude skill 转换为 Codex skill 格式，归档到 .codex/skills 目录。当用户要求将 Claude skill 转为 Codex skill、迁移 skill 到 Codex 时触发。
argument-hint: <skill-name>
allowed-tools:
  - Read
  - Write
  - Bash
  - Glob
  - Grep
---

# Claude Skill → Codex Skill 转换器

将当前项目 `.claude/skills/` 下指定的 Claude skill 转换为 Codex skill 格式，归档到 `.codex/skills/` 目录。

目标 skill 名称：**$ARGUMENTS**

## 格式差异说明

### Claude skill 格式（源）

```
.claude/skills/<name>/SKILL.md
```

- YAML frontmatter 字段：`name`、`description`、`argument-hint`（可选）、`allowed-tools`（可选）、`disable-model-invocation`（可选）
- 参数占位符：`$ARGUMENTS[0]`、`$ARGUMENTS[1]` 等
- 无 `agents/` 子目录

### Codex skill 格式（目标）

```
.codex/skills/<name>/SKILL.md
.codex/skills/<name>/agents/openai.yaml
```

- YAML frontmatter 字段：仅 `name`、`description`
  - `description` 中追加触发说明：`或使用 $<skill-name> 时触发`
  - 移除 `argument-hint`、`allowed-tools`、`disable-model-invocation`
- 参数占位符：将 `$ARGUMENTS[0]`、`$ARGUMENTS[1]` 等替换为自然语言描述
  - 如果原 skill 有 `argument-hint`，在正文中添加一段说明：`用户通过参数指定，格式：<argument-hint 的内容>`
- 生成 `agents/openai.yaml`，格式：
  ```yaml
  interface:
    display_name: "<从 SKILL.md 标题或 name 提取>"
    short_description: "<从 description 提炼一句话>"
    default_prompt: "Use $<skill-name> to <简述功能>."
  ```

## 执行步骤

### 步骤 1：读取源 skill

读取 `.claude/skills/$ARGUMENTS/SKILL.md` 的完整内容，解析 frontmatter 和正文。

### 步骤 2：转换 frontmatter

1. 保留 `name` 和 `description`
2. 在 `description` 末尾追加 `或使用 $<skill-name> 时触发`（如果尚未包含）
3. 移除 `argument-hint`、`allowed-tools`、`disable-model-invocation` 等 Claude 专有字段

### 步骤 3：转换正文

1. 将所有 `$ARGUMENTS[N]` 替换为通用描述（结合 `argument-hint` 上下文）
2. 如果原 skill 有 `argument-hint`，在正文开头（标题之后）添加参数说明段落

### 步骤 4：生成 agents/openai.yaml

根据 skill 的 `name`、`description` 和正文标题生成 `agents/openai.yaml`。

### 步骤 5：写入目标文件

1. 创建 `.codex/skills/<name>/` 和 `.codex/skills/<name>/agents/` 目录
2. 写入转换后的 `SKILL.md`
3. 写入 `agents/openai.yaml`

### 步骤 6：如果目标目录下已有同名 skill，还包含其他文件（如 scripts/）

将源 skill 目录下除 SKILL.md 外的其他文件/目录也一并复制到目标。

### 步骤 7：输出结果

列出生成的文件清单，提示用户检查。
