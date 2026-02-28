<?php
/**
 * @file
 * Template for the quiz-taking page.
 *
 * Available variables:
 * - $quiz_title: The quiz title (escaped).
 * - $form: The quiz form render array; print with drupal_render($form).
 * - $progress: Optional progress indicator string.
 * - $time_limit: Time limit in seconds; 0 = no limit.
 */
?>
<div class="quiz-kit-take">
  <h2><?php print $quiz_title; ?></h2>

  <?php if ($time_limit): ?>
    <div class="quiz-kit-timer" data-time-limit="<?php print (int) $time_limit; ?>">
      <?php print t('Time remaining: <span class="quiz-kit-countdown"></span>'); ?>
    </div>
  <?php endif; ?>

  <?php if ($progress): ?>
    <div class="quiz-kit-progress"><?php print $progress; ?></div>
  <?php endif; ?>

  <?php print drupal_render($form); ?>
</div>
