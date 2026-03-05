# Quiz Kit

Quiz Kit is a quiz and survey module for Backdrop CMS. It allows site builders to create quizzes with multiple question types, organize questions into sections, and review results — all without leaving the Backdrop admin interface.

## Getting Started

1. Install and enable the module (see [Installation](#installation) below).
2. Go to **Admin > Quiz Kit** to configure default settings.
3. Create a new piece of content using the **Quiz** content type.
4. On the quiz node, use the **Sections** tab to add and order sections (optional).
5. Use the **Questions** tab to add questions to the quiz.
6. Publish the quiz so users can take it.

Users with the **Take quizzes** permission will see a "Take Quiz" button on the quiz page. Results are stored per user and can be reviewed on the **Results** tab.

---

## Features

### Question Types
- **Multiple choice — one correct answer** (`mc_single`): Respondents choose one option from a list.
- **Multiple choice — any one correct** (`mc_any`): Multiple options may be correct; choosing any one passes.
- **Multiple choice — all correct answers required** (`mc_multi`): Respondents select all that apply; all correct answers must be chosen.
- **True / False** (`truefalse`): A simple two-option question.
- **Short text** (`short_text`): Respondents type a brief answer; graded by exact match (case-insensitive).
- **Long text** (`long_text`): An open-ended textarea response. Not graded; useful for collecting metadata such as name, class, or comments.

### Answer Display
Each question has an **Answer display** setting (Horizontal or Vertical, default Horizontal) that controls how answer choices are laid out for quiz-takers.

### Scoring
- Each question has an **Include in score** toggle. Uncheck it to collect a response without counting it toward the score (useful for name, class, or other metadata questions).
- A configurable **pass rate** (percentage) determines whether a result is marked as passed or failed. This can be set site-wide and overridden per quiz.

### Sections
Questions can be grouped into named sections. Each section supports:
- A **title** and optional **instructions** shown to quiz-takers.
- A **page break** option that places the section on its own page with a Next button.
- **Require answers before advancing** — when enabled, quiz-takers must answer all required questions on the page before the Next button will proceed.
- **Prevent going back once advanced** — when enabled, quiz-takers cannot use the Back button to return to this section or any earlier page after advancing past it.

### Multi-page Navigation
When sections have page breaks, the quiz is split into multiple pages. Quiz-takers can freely move forward and back between pages (unless a section lock prevents it). Partially entered answers are preserved when navigating between pages.

### Time Limit
Quizzes can have an optional time limit (in minutes). When a time limit is set:
- A countdown timer is displayed throughout the quiz, persisting across page navigation.
- The timer turns red when less than 60 seconds remain.
- When time expires, the quiz is automatically submitted and graded with whatever answers have been provided.
- Time limits are enforced server-side to prevent circumvention.
- A site-wide default can be set, with per-quiz overrides.

### Per-quiz Settings
The following can be configured on each quiz node, overriding site-wide defaults where applicable:
- **Pass rate (%)** — score required to pass.
- **Maximum attempts** — how many times a user may take the quiz (0 = unlimited).
- **Time limit (minutes)** — maximum time to complete the quiz (0 = no limit).
- **Show section scores** — displays a score breakdown by section on the results page.
- **Require all answers** — quiz-takers must answer every question before submitting.

### Question Bank
All questions across all quizzes are listed at **Admin > Quiz Kit > Question bank**. Questions are stored independently of quizzes via a junction table, making it possible to add an existing question to multiple quizzes using the **Add existing question** action on the Questions tab.

### Results
- Results are listed on the **Results** tab of each quiz node (visible to quiz editors and users with the "View any quiz results" permission).
- Each result shows the user's score, pass/fail status, date, and a detailed answer review.
- The answer review table highlights incorrect answers and (optionally) shows the correct answer.
- Quiz editors can **delete individual results**, for example to remove test attempts.
- The results list is paginated; page size is configurable in the global settings.

#### Show Answer in Results List
Individual questions can be flagged with **Show answer in results list** (up to 2 per quiz). When enabled, that question's answer appears as an extra column in the results table — useful for metadata questions like "Name" or "Class" that help identify respondents at a glance.

### Unpublished Quiz Preview
Editors with permission to edit a quiz can take it while it is unpublished, allowing them to test it before making it available to users. A warning banner reminds editors that they are previewing an unpublished quiz.

### Permissions
| Permission | Description |
|---|---|
| Administer Quiz Kit | Access global settings and view all results. |
| Take quizzes | Attempt published quizzes and submit answers. |
| View own quiz results | View personal result history. |
| View any quiz results | View results for all users. |

---

## Notes About Use of AI

This module was developed with significant assistance from AI tools (specifically Claude by Anthropic). AI was used to generate code, plan features, and make iterative improvements throughout development.

We encourage folks to review the code and help us identify quirky things that AI might have done or places where it may not be following Backdrop CMS best practices. Pull requests and issue reports are welcome.

---

## Installation

Install this module using the official Backdrop CMS instructions at https://backdropcms.org/guide/modules

---

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.

---

## Current Maintainers

- Tim Erickson ([@stpaultim](https://github.com/stpaultim))
- Seeking additional maintainers.
