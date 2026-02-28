<?php
/**
 * @file
 * Template for the quiz results page.
 *
 * Available variables:
 * - $quiz_title: The quiz title (escaped).
 * - $result: The result object (score, passed, finished, etc.).
 * - $score: Numeric score percentage.
 * - $passed: Boolean.
 * - $pass_message: Localised pass/fail message string.
 * - $date_taken: Formatted date the quiz was submitted.
 * - $answers: Array of answer objects with question_text, answer_given, is_correct.
 * - $show_correct: Boolean — whether to reveal correct answers.
 */
?>
<div class="quiz-kit-results <?php print $passed ? 'quiz-kit-passed' : 'quiz-kit-failed'; ?>">
  <h2><?php print $quiz_title; ?></h2>

  <div class="quiz-kit-score">
    <p><?php print t('Score: <strong>@score%</strong>', array('@score' => $score)); ?></p>
    <p class="quiz-kit-pass-message"><?php print $pass_message; ?></p>
    <p><?php print t('Completed: @date', array('@date' => $date_taken)); ?></p>
  </div>

  <?php if ($show_correct && $answers): ?>
    <div class="quiz-kit-answer-review">
      <h3><?php print t('Review'); ?></h3>
      <?php foreach ($answers as $a): ?>
        <div class="quiz-kit-answer-item <?php print $a->is_correct ? 'correct' : 'incorrect'; ?>">
          <p class="quiz-kit-question-text"><?php print check_plain($a->question_text); ?></p>
          <p class="quiz-kit-answer-given">
            <?php print t('Your answer: @answer', array('@answer' => $a->answer_given)); ?>
            <?php print $a->is_correct ? '✓' : '✗'; ?>
          </p>
        </div>
      <?php endforeach; ?>
    </div>
  <?php endif; ?>
</div>
