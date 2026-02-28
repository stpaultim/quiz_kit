<?php
/**
 * @file
 * Theme functions for Quiz Kit.
 *
 * Most output is handled by template files in templates/. This file is
 * reserved for preprocess functions and any theme functions that are simpler
 * to write in PHP than in a template.
 */

/**
 * Preprocesses variables for quiz-kit-take.tpl.php.
 */
function template_preprocess_quiz_kit_take(&$variables) {
  $node = $variables['node'];
  $variables['quiz_title'] = check_plain($node->title);
  $variables['time_limit'] = (int) _quiz_kit_node_setting($node, 'time_limit', 0);
}

/**
 * Preprocesses variables for quiz-kit-results.tpl.php.
 */
function template_preprocess_quiz_kit_results(&$variables) {
  $node = $variables['node'];
  $result = $variables['result'];

  $variables['quiz_title'] = check_plain($node->title);
  $variables['score'] = $result->score;
  $variables['passed'] = (bool) $result->passed;
  $variables['pass_message'] = $result->passed ? t('Congratulations — you passed!') : t('You did not pass. Better luck next time.');
  $variables['date_taken'] = format_date($result->finished, 'medium');
}
