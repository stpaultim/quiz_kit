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
 * - $total_count: Total number of questions answered.
 * - $correct_count: Number of questions answered correctly.
 * - $answers: Array of answer objects with question_text, answer_display, is_correct.
 * - $show_correct: Boolean — whether to reveal the answer review table.
 * - $section_scores: Array of objects with title and score properties.
 * - $show_section_scores: Boolean — whether to show the section breakdown.
 */
?>
<div class="quiz-kit-results <?php print $passed ? 'quiz-kit-passed' : 'quiz-kit-failed'; ?>">

  <div class="quiz-kit-score">
    <p class="quiz-kit-pass-message"><?php print $pass_message; ?></p>
    <p><?php print t('Score: <strong>@score%</strong> (@correct of @total correct)', array(
      '@score'   => $score,
      '@correct' => $correct_count,
      '@total'   => $total_count,
    )); ?></p>
    <p><?php print t('Completed: @date', array('@date' => $date_taken)); ?></p>
  </div>

  <?php if ($show_section_scores && $section_scores): ?>
    <div class="quiz-kit-section-scores">
      <h3><?php print t('Section scores'); ?></h3>
      <ul>
        <?php foreach ($section_scores as $ss): ?>
          <li>
            <span class="quiz-kit-section-score-title"><?php print check_plain($ss->title); ?></span>:
            <strong><?php print $ss->score; ?>%</strong>
          </li>
        <?php endforeach; ?>
      </ul>
    </div>
  <?php endif; ?>

  <?php if ($show_correct && $answers): ?>
    <div class="quiz-kit-answer-review">
      <h3><?php print t('Review'); ?></h3>
      <table class="quiz-kit-results-table">
        <thead>
          <tr>
            <th><?php print t('Question'); ?></th>
            <th><?php print t('Your answer'); ?></th>
            <th><?php print t('Status'); ?></th>
          </tr>
        </thead>
        <tbody>
          <?php $n = 1; foreach ($answers as $a): ?>
          <?php
            if (!$a->scored):
              $row_class = 'quiz-kit-not-scored';
            elseif ($a->is_correct):
              $row_class = 'quiz-kit-correct';
            else:
              $row_class = 'quiz-kit-incorrect';
            endif;
          ?>
          <tr class="<?php print $row_class; ?>">
            <td><?php print $n . '. ' . check_plain($a->question_text); ?></td>
            <td>
              <?php print check_plain($a->answer_display); ?>
              <?php if ($a->scored && !$a->is_correct && !empty($a->correct_answer_display)): ?>
                <br><em class="quiz-kit-correct-answer"><?php print t('Correct: @answer', array('@answer' => check_plain($a->correct_answer_display))); ?></em>
              <?php endif; ?>
            </td>
            <td class="quiz-kit-status">
              <?php if (!$a->scored): ?>
                <span class="quiz-kit-not-scored-label"><?php print t('—'); ?></span>
              <?php elseif ($a->is_correct): ?>
                <span class="quiz-kit-correct-label"><?php print t('Correct'); ?></span>
              <?php else: ?>
                <span class="quiz-kit-incorrect-label"><?php print t('Incorrect'); ?></span>
              <?php endif; ?>
            </td>
          </tr>
          <?php $n++; endforeach; ?>
        </tbody>
      </table>
    </div>
  <?php endif; ?>

</div>
