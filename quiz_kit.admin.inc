<?php
/**
 * @file
 * Admin pages: global settings, question management UI.
 */

// ---------------------------------------------------------------------------
// Global settings
// ---------------------------------------------------------------------------

/**
 * Form: global Quiz Kit settings.
 */
function quiz_kit_admin_settings_form($form, &$form_state) {
  $form['quiz_kit_default_pass_rate'] = array(
    '#type' => 'textfield',
    '#title' => t('Default pass rate (%)'),
    '#default_value' => config_get('quiz_kit.settings', 'default_pass_rate') ?: 75,
    '#size' => 5,
    '#description' => t('Percentage score required to pass a quiz. Can be overridden per quiz.'),
  );

  $form['quiz_kit_default_max_attempts'] = array(
    '#type' => 'textfield',
    '#title' => t('Default maximum attempts'),
    '#default_value' => config_get('quiz_kit.settings', 'default_max_attempts') ?: 0,
    '#size' => 5,
    '#description' => t('How many times a user may take a quiz. 0 = unlimited. Can be overridden per quiz.'),
  );

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array('#type' => 'submit', '#value' => t('Save configuration'));

  return $form;
}

/**
 * Submit: global settings form.
 */
function quiz_kit_admin_settings_form_submit($form, &$form_state) {
  config_set('quiz_kit.settings', 'default_pass_rate', (int) $form_state['values']['quiz_kit_default_pass_rate']);
  config_set('quiz_kit.settings', 'default_max_attempts', (int) $form_state['values']['quiz_kit_default_max_attempts']);
  backdrop_set_message(t('Quiz Kit settings saved.'));
}

// ---------------------------------------------------------------------------
// Question list page
// ---------------------------------------------------------------------------

/**
 * Page callback: list questions for a quiz node.
 */
function quiz_kit_questions_page($node) {
  $questions = db_query(
    'SELECT q.*, r.question_text FROM {quiz_kit_question} q
     JOIN {quiz_kit_question_revision} r ON r.vid = q.vid
     WHERE q.nid = :nid ORDER BY q.weight, q.id',
    array(':nid' => $node->nid)
  )->fetchAll();

  if (empty($questions)) {
    return t('No questions yet. <a href="@url">Add the first question.</a>', array(
      '@url' => url('node/' . $node->nid . '/questions/add'),
    ));
  }

  $header = array(t('Question'), t('Type'), t('Operations'));
  $rows = array();
  foreach ($questions as $q) {
    $ops = array(
      l(t('Edit'), 'node/' . $node->nid . '/questions/' . $q->id . '/edit'),
      l(t('Delete'), 'node/' . $node->nid . '/questions/' . $q->id . '/delete'),
    );
    $rows[] = array(
      check_plain($q->question_text),
      check_plain($q->type),
      implode(' | ', $ops),
    );
  }

  return theme('table', array('header' => $header, 'rows' => $rows));
}

// ---------------------------------------------------------------------------
// Question add/edit form
// ---------------------------------------------------------------------------

/**
 * Form: add or edit a question.
 *
 * @param object $node
 *   The quiz node.
 * @param object|null $question
 *   Existing question object for edits; NULL for new questions.
 */
function quiz_kit_question_form($form, &$form_state, $node, $question = NULL) {
  $form_state['quiz_node'] = $node;
  $form_state['question'] = $question;

  $form['type'] = array(
    '#type' => 'select',
    '#title' => t('Question type'),
    '#options' => quiz_kit_question_type_options(),
    '#default_value' => $question ? $question->type : 'mc_single',
    '#required' => TRUE,
    '#disabled' => (bool) $question,
    '#description' => $question ? t('Question type cannot be changed after creation.') : '',
  );

  $form['question_text'] = array(
    '#type' => 'textarea',
    '#title' => t('Question'),
    '#default_value' => $question ? _quiz_kit_current_question_text($question) : '',
    '#required' => TRUE,
    '#rows' => 4,
  );

  // Answer fields are rendered via AJAX or always shown; for the scaffold
  // they are a placeholder — the full answer UI will be built out next.
  $form['answers_placeholder'] = array(
    '#markup' => '<p>' . t('Answer options will appear here.') . '</p>',
  );

  $form['weight'] = array(
    '#type' => 'textfield',
    '#title' => t('Weight'),
    '#default_value' => $question ? $question->weight : 0,
    '#size' => 5,
    '#description' => t('Lower numbers appear first.'),
  );

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => $question ? t('Save question') : t('Add question'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'node/' . $node->nid . '/questions'),
  );

  return $form;
}

/**
 * Submit: question add/edit form.
 */
function quiz_kit_question_form_submit($form, &$form_state) {
  $node = $form_state['quiz_node'];
  $question = $form_state['question'];
  $values = $form_state['values'];
  $now = REQUEST_TIME;

  if ($question) {
    // Create a new revision of the existing question.
    $vid = db_insert('quiz_kit_question_revision')
      ->fields(array(
        'id' => $question->id,
        'question_text' => $values['question_text'],
        'created' => $now,
      ))
      ->execute();

    db_update('quiz_kit_question')
      ->fields(array(
        'vid' => $vid,
        'weight' => (int) $values['weight'],
        'changed' => $now,
      ))
      ->condition('id', $question->id)
      ->execute();

    backdrop_set_message(t('Question updated.'));
  }
  else {
    // Insert the question record first to get its id.
    $qid = db_insert('quiz_kit_question')
      ->fields(array(
        'nid' => $node->nid,
        'vid' => 0,
        'type' => $values['type'],
        'weight' => (int) $values['weight'],
        'created' => $now,
        'changed' => $now,
      ))
      ->execute();

    // Insert the first revision.
    $vid = db_insert('quiz_kit_question_revision')
      ->fields(array(
        'id' => $qid,
        'question_text' => $values['question_text'],
        'created' => $now,
      ))
      ->execute();

    // Update the question with the new vid.
    db_update('quiz_kit_question')
      ->fields(array('vid' => $vid))
      ->condition('id', $qid)
      ->execute();

    backdrop_set_message(t('Question added.'));
  }

  $form_state['redirect'] = 'node/' . $node->nid . '/questions';
}

// ---------------------------------------------------------------------------
// Question delete form
// ---------------------------------------------------------------------------

/**
 * Form: confirm question deletion.
 */
function quiz_kit_question_delete_form($form, &$form_state, $node, $question) {
  $form_state['quiz_node'] = $node;
  $form_state['question'] = $question;

  return confirm_form(
    $form,
    t('Are you sure you want to delete this question?'),
    'node/' . $node->nid . '/questions',
    t('This action cannot be undone. All revisions and stored answers for this question will be deleted.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit: question delete confirm form.
 */
function quiz_kit_question_delete_form_submit($form, &$form_state) {
  $question = $form_state['question'];
  $node = $form_state['quiz_node'];

  // Delete answers for all revisions of this question.
  $vids = db_query('SELECT vid FROM {quiz_kit_question_revision} WHERE id = :id', array(':id' => $question->id))->fetchCol();
  if ($vids) {
    db_delete('quiz_kit_answer')->condition('question_vid', $vids, 'IN')->execute();
  }
  db_delete('quiz_kit_question_revision')->condition('id', $question->id)->execute();
  db_delete('quiz_kit_question')->condition('id', $question->id)->execute();

  backdrop_set_message(t('Question deleted.'));
  $form_state['redirect'] = 'node/' . $node->nid . '/questions';
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Returns human-readable labels for question types.
 */
function quiz_kit_question_type_options() {
  return array(
    'mc_single' => t('Multiple choice — single answer'),
    'mc_multi' => t('Multiple choice — multiple answers'),
    'truefalse' => t('True / False'),
    'short_text' => t('Short text (exact match)'),
  );
}

/**
 * Returns the question text for the current revision of a question.
 */
function _quiz_kit_current_question_text($question) {
  return db_query('SELECT question_text FROM {quiz_kit_question_revision} WHERE vid = :vid', array(':vid' => $question->vid))->fetchField();
}
