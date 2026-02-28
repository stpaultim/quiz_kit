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
 * Theme function: renders the questions management table with tabledrag.
 *
 * Called via #theme = 'quiz_kit_questions_table' on the questions container
 * in quiz_kit_questions_form(). Iterates over each question row element,
 * builds a draggable table, and wires up Backdrop's tabledrag behaviour.
 */
function theme_quiz_kit_questions_table($variables) {
  $form = $variables['form'];
  $node = $form['#node'];

  $header = array(
    // Empty column for the drag handle (inserted by tabledrag JS).
    array('data' => '', 'class' => array('quiz-kit-drag-handle-col')),
    t('Question'),
    t('Type'),
    // Weight column is hidden visually; tabledrag uses it internally.
    array('data' => t('Weight'), 'class' => array('tabledrag-hide')),
    t('Operations'),
  );

  $rows = array();
  foreach (element_children($form) as $qid) {
    $element = &$form[$qid];

    // Ensure the weight field carries the targeting class.
    $element['weight']['#attributes']['class'][] = 'quiz-kit-question-weight';

    $rows[] = array(
      'data' => array(
        // Drag handle cell — tabledrag JS inserts the handle icon here.
        array('class' => array('quiz-kit-drag-col')),
        array('data' => drupal_render($element['question_text'])),
        array('data' => drupal_render($element['type_label'])),
        array(
          'data'  => drupal_render($element['weight']),
          'class' => array('tabledrag-hide'),
        ),
        array('data' => drupal_render($element['ops'])),
      ),
      'class' => array('draggable'),
    );
  }

  backdrop_add_tabledrag(
    'quiz-kit-questions-table',
    'order',
    'sibling',
    'quiz-kit-question-weight'
  );

  return theme('table', array(
    'header'     => $header,
    'rows'       => $rows,
    'attributes' => array('id' => 'quiz-kit-questions-table'),
    'empty'      => t('No questions yet.'),
  ));
}

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
