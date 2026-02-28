# Quiz Kit — Module Plan

**Created:** 2026-02-28
**Status:** Phase 1 in progress
**Machine name:** `quiz_kit`

---

## Overview

Quiz Kit is a Backdrop CMS module that allows content creators to build quizzes with multiple question types, and allows users to take quizzes and view their results. Quizzes are built on Backdrop's node system, which gives editors a familiar starting point and leverages existing node access, publishing, and workflow features.

---

## Architecture

### Quizzes as Nodes
Quizzes are a custom **node type** (`quiz_kit_quiz`). This means:
- Site editors create and manage quizzes through the standard node add/edit interface
- Publishing, unpublishing, revisions, and node access control work out of the box
- The node title serves as the quiz title; the body field (or a custom summary field) serves as the description

### Questions and Answers as Custom Entities
Questions and answers are **not** nodes — they are lightweight custom entities (or plain database records) that belong to a quiz node. This keeps the node system uncluttered while still giving quizzes the familiar node-editing experience.

### Anonymous Attempts (Future)
Anonymous quiz attempts are a planned future feature. Anonymous results will be stored in the database, but anonymous users will have no way to revisit their score after leaving the page (no persistent session linking). This should be kept in mind during schema and permission design so it can be enabled without a data migration.

---

## Core Features

### Quiz Management (Admin/Author)
- Create and manage quizzes as nodes (node type: `quiz_kit_quiz`)
- Add/edit/reorder questions via a dedicated question management UI on the node
- Set quiz options per node: time limit, pass/fail threshold, number of attempts allowed, randomize question order, randomize answer order
- Publish/unpublish via standard node controls

### Question Types — Phase 1
- Multiple choice (single answer)
- Multiple choice (multiple answers)
- True/False
- Short text answer (exact match — auto-graded)

### Question Types — Phase 2 (later)
- Matching (drag and drop or dropdowns)
- Fill in the blank
- Image-based questions

### Taking a Quiz (User)
- Paginated question display (one at a time or all at once — configurable per quiz)
- Progress indicator
- Timer display (if time limit set)
- Submit for grading

### Results & Feedback
- Immediate scoring for auto-graded questions
- Show correct/incorrect answers (configurable — can be disabled per quiz)
- Pass/fail message with score percentage
- Result history per user

---

## Data Structure

### Node Type
| Field | Details |
|---|---|
| `quiz_kit_quiz` | Standard node type; title = quiz title, body = description |
| `field_quiz_category` | Taxonomy term reference — `quiz_kit_categories` vocabulary. Admin-only organizational field; not shown to quiz-takers. Single value. |
| `field_quiz_time_limit` | Integer field (seconds); 0 = no limit |
| `field_quiz_pass_rate` | Integer field (percentage, 0–100) |
| `field_quiz_max_attempts` | Integer field; 0 = unlimited |
| `field_quiz_randomize_questions` | Boolean field |
| `field_quiz_randomize_answers` | Boolean field |
| `field_quiz_show_answers` | Boolean — show correct answers after submission |
| `field_quiz_questions_per_page` | Integer; 0 = show all on one page |

### Quiz Categories Taxonomy
A vocabulary named **Quiz Categories** (`quiz_kit_categories`) is created by the module on install. Site admins manage terms at `admin/structure/taxonomy/quiz_kit_categories`. The vocabulary is deleted on uninstall.

The category field is for admin organisation only — grouping quizzes by audience, topic, or campaign. It is not part of the quiz-taking experience and is not shown to end users.

### Custom Tables

| Table | Key Fields |
|---|---|
| `quiz_kit_question` | id, vid (current revision), nid, type, question_text, weight, created, changed |
| `quiz_kit_question_revision` | vid, id (question), question_text, created |
| `quiz_kit_answer` | id, question_vid, answer_text, is_correct, weight |
| `quiz_kit_result` | id, nid (quiz node), uid, score, passed, started, finished |
| `quiz_kit_result_answer` | id, result_id, question_id, question_vid, answer_given, is_correct |

**Question versioning:** When a question is edited, a new row is written to `quiz_kit_question_revision` and `quiz_kit_question.vid` is updated. Answers (`quiz_kit_answer`) are keyed to `question_vid`, not `question_id`, so answer sets are immutable per revision. `quiz_kit_result_answer` stores the `question_vid` that was active during the attempt, ensuring past results always reflect what the user actually saw — even if the question is later edited.

**Anonymous future note:** `uid` in `quiz_kit_result` allows 0 (anonymous). A session token column can be added later to link anonymous attempts to a browser session without requiring a data migration.

---

## Module File Structure

```
quiz_kit/
├── plan.md                      (this file)
├── quiz_kit.info
├── quiz_kit.module
├── quiz_kit.install              (schema + node type definition)
├── quiz_kit.admin.inc            (admin pages, question management UI)
├── quiz_kit.pages.inc            (take quiz, results pages)
├── quiz_kit.theme.inc            (theme function definitions)
├── js/
│   └── quiz_kit.js               (timer, progress, UX interactions)
├── css/
│   └── quiz_kit.css
└── templates/
    ├── quiz-kit-take.tpl.php
    └── quiz-kit-results.tpl.php
```

---

## Permissions

| Permission | Description |
|---|---|
| `administer quiz_kit` | Full admin access to Quiz Kit settings |
| `create quiz_kit_quiz content` | Standard node permission — create quiz nodes |
| `edit own quiz_kit_quiz content` | Standard node permission |
| `edit any quiz_kit_quiz content` | Standard node permission |
| `delete own quiz_kit_quiz content` | Standard node permission |
| `delete any quiz_kit_quiz content` | Standard node permission |
| `take quizzes` | Users can attempt quizzes |
| `view own quiz results` | See own result history |
| `view any quiz results` | Admins/instructors see all results |

---

## Menu / Routes

| Path | Purpose |
|---|---|
| `admin/content/quiz-kit` | List all quiz nodes (or link to content admin filtered by type) |
| `admin/config/quiz-kit` | Module-level settings |
| `node/add/quiz-kit-quiz` | Standard node add (create a quiz) |
| `node/%/questions` | Manage questions for a quiz node |
| `node/%/questions/add` | Add a question |
| `node/%/questions/%/edit` | Edit a question |
| `quiz` | List available quizzes |
| `quiz/%node/take` | Take a quiz (% = quiz node id) |
| `quiz/%node/results` | Results list for a quiz (admin/author) |
| `quiz/%node/results/%` | Individual result detail |
| `user/%/quiz-results` | A user's own result history |

---

## Nice-to-Haves (Backlog)

- Block: "Featured Quiz" or "Quiz of the Day"
- Views integration for listing quizzes and results
- Certificate generation on pass
- Question bank (reuse questions across quizzes)
- Import questions from CSV
- Feedback per answer (show explanation after submitting)
- Anonymous attempt support (see Architecture notes above)

---

## Reference

The [Drupal Quiz module](https://www.drupal.org/project/quiz) was reviewed for feature inspiration. Key ideas noted: question versioning (adopted), question pools/randomization from pools (Phase 2 backlog), adaptive mode with per-question retries (backlog), build-on-last-attempt UX (backlog). Its Drupal 10 OOP architecture does not apply to Backdrop.

---

## Open Questions

- Should `quiz_kit_question` be a proper Backdrop entity type, or plain database records managed by custom forms? Entity type gives Views integration for free but adds complexity.
- Display format for taking a quiz: one question per page vs. all on one page — default overridable per quiz, or a global setting?
