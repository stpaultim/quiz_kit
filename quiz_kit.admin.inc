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

  $form['quiz_kit_default_answer_count'] = array(
    '#type' => 'select',
    '#title' => t('Default number of answer options'),
    '#options' => array(2 => 2, 3 => 3, 4 => 4, 5 => 5, 6 => 6, 7 => 7, 8 => 8),
    '#default_value' => config_get('quiz_kit.settings', 'default_answer_count') ?: 3,
    '#description' => t('How many answer rows to show by default when creating a multiple choice question. Applies to single- and multiple-answer question types only.'),
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
  config_set('quiz_kit.settings', 'default_answer_count', (int) $form_state['values']['quiz_kit_default_answer_count']);
  backdrop_set_message(t('Quiz Kit settings saved.'));
}

// ---------------------------------------------------------------------------
// Question list / ordering form
// ---------------------------------------------------------------------------

/**
 * Form: list questions for a quiz with tabledrag reordering.
 */
function quiz_kit_questions_form($form, &$form_state, $node) {
  $form_state['quiz_node'] = $node;

  $questions = db_query(
    'SELECT q.id, q.weight, q.type, r.question_text
     FROM {quiz_kit_question} q
     JOIN {quiz_kit_question_revision} r ON r.vid = q.vid
     WHERE q.nid = :nid
     ORDER BY q.weight, q.id',
    array(':nid' => $node->nid)
  )->fetchAll();

  if (empty($questions)) {
    $form['empty'] = array(
      '#markup' => t('No questions yet. <a href="@url">Add the first question.</a>', array(
        '@url' => url('node/' . $node->nid . '/questions/add'),
      )),
    );
    return $form;
  }

  $type_labels = quiz_kit_question_type_options();
  $delta = count($questions);

  $form['questions'] = array(
    '#tree'  => TRUE,
    '#theme' => 'quiz_kit_questions_table',
    '#node'  => $node,
  );

  foreach ($questions as $q) {
    $form['questions'][$q->id] = array(
      'question_text' => array(
        '#markup' => check_plain($q->question_text),
      ),
      'type_label' => array(
        '#markup' => isset($type_labels[$q->type]) ? $type_labels[$q->type] : check_plain($q->type),
      ),
      'weight' => array(
        '#type'          => 'weight',
        '#title'         => t('Weight'),
        '#title_display' => 'invisible',
        '#default_value' => $q->weight,
        '#delta'         => $delta,
        '#attributes'    => array('class' => array('quiz-kit-question-weight')),
      ),
      'ops' => array(
        '#markup' => implode(' | ', array(
          l(t('Edit'), 'node/' . $node->nid . '/questions/' . $q->id . '/edit'),
          l(t('Delete'), 'node/' . $node->nid . '/questions/' . $q->id . '/delete'),
        )),
      ),
    );
  }

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Save order'),
  );

  return $form;
}

/**
 * Submit: save the reordered question weights.
 */
function quiz_kit_questions_form_submit($form, &$form_state) {
  foreach ($form_state['values']['questions'] as $qid => $values) {
    db_update('quiz_kit_question')
      ->fields(array('weight' => (int) $values['weight']))
      ->condition('id', $qid)
      ->execute();
  }
  backdrop_set_message(t('Question order saved.'));
}

// ---------------------------------------------------------------------------
// Question add/edit form
// ---------------------------------------------------------------------------

/**
 * Form: add or edit a question.
 *
 * @param object $node   The quiz node.
 * @param object|null $question  Existing question for edits; NULL for new.
 */
function quiz_kit_question_form($form, &$form_state, $node, $question = NULL) {
  $form_state['quiz_node'] = $node;
  $form_state['question']  = $question;

  // Resolve the active type using the most reliable source available.
  // Priority: persistent form_state storage > processed values (normal submit /
  // type-change AJAX) > raw POST input (add-answer AJAX, which skips
  // validation so $form_state['values'] is empty) > existing question > default.
  $known_types = array_keys(quiz_kit_question_type_options());
  if (isset($form_state['quiz_question_type'])) {
    // Already resolved on a previous build; check whether it was overridden
    // by a type-change AJAX (values will be present in that case).
    if (!empty($form_state['values']['type']) && in_array($form_state['values']['type'], $known_types)) {
      $type = $form_state['values']['type'];
    }
    else {
      $type = $form_state['quiz_question_type'];
    }
  }
  elseif (!empty($form_state['values']['type']) && in_array($form_state['values']['type'], $known_types)) {
    $type = $form_state['values']['type'];
  }
  elseif (!empty($form_state['input']['type']) && in_array($form_state['input']['type'], $known_types)) {
    // Raw POST — present when #limit_validation_errors suppresses processing.
    $type = $form_state['input']['type'];
  }
  elseif ($question) {
    $type = $question->type;
  }
  else {
    $type = 'mc_single';
  }

  // Persist so subsequent rebuilds (e.g. "Add another answer") keep the type.
  $form_state['quiz_question_type'] = $type;

  // Load existing answers (edit mode only).
  $existing_answers = $question ? _quiz_kit_load_answers_for_vid($question->vid) : array();

  // Reset the answer row count if the type changed since the last build.
  if (isset($form_state['prev_question_type']) && $form_state['prev_question_type'] !== $type) {
    unset($form_state['answer_count']);
  }
  $form_state['prev_question_type'] = $type;

  // Initialise the answer row count once per type.
  if (!isset($form_state['answer_count'])) {
    if ($existing_answers) {
      $form_state['answer_count'] = count($existing_answers);
    }
    else {
      $default = config_get('quiz_kit.settings', 'default_answer_count') ?: 3;
      $form_state['answer_count'] = in_array($type, array('mc_single', 'mc_multi')) ? (int) $default : 1;
    }
  }

  // --- Question type ---
  $form['type'] = array(
    '#type'          => 'select',
    '#title'         => t('Question type'),
    '#options'       => quiz_kit_question_type_options(),
    '#default_value' => $type,
    '#required'      => TRUE,
    '#disabled'      => (bool) $question,
    '#description'   => $question ? t('Question type cannot be changed after creation.') : '',
  );
  // On new questions, changing the type reloads the answer UI via AJAX.
  if (!$question) {
    $form['type']['#ajax'] = array(
      'callback' => 'quiz_kit_answers_ajax_callback',
      'wrapper'  => 'quiz-kit-answers-wrapper',
      'effect'   => 'fade',
    );
  }

  // --- Question text ---
  $form['question_text'] = array(
    '#type' => 'textarea',
    '#title' => t('Question'),
    '#default_value' => $question ? _quiz_kit_current_question_text($question) : '',
    '#required' => TRUE,
    '#rows' => 4,
  );

  // --- Answers (AJAX-replaceable container) ---
  $form['answers'] = array(
    '#type'   => 'container',
    '#tree'   => TRUE,
    '#prefix' => '<div id="quiz-kit-answers-wrapper">',
    '#suffix' => '</div>',
  );
  _quiz_kit_build_answer_ui($form, $form_state, $type, $existing_answers);

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $question ? t('Save question') : t('Add question'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'node/' . $node->nid . '/questions'),
  );

  return $form;
}

/**
 * Validate: question form.
 */
function quiz_kit_question_form_validate($form, &$form_state) {
  $type = $form_state['question']
    ? $form_state['question']->type
    : $form_state['values']['type'];

  if (in_array($type, array('mc_single', 'mc_multi'))) {
    $filled = 0;
    foreach ($form_state['values']['answers']['rows'] as $row) {
      if (!empty($row['text'])) {
        $filled++;
      }
    }
    if ($filled < 2) {
      form_set_error('answers', t('Please provide at least two answer options.'));
    }
    if ($type === 'mc_multi') {
      $has_correct = FALSE;
      foreach ($form_state['values']['answers']['rows'] as $row) {
        if (!empty($row['text']) && !empty($row['is_correct'])) {
          $has_correct = TRUE;
          break;
        }
      }
      if (!$has_correct) {
        form_set_error('answers', t('Please mark at least one answer as correct.'));
      }
    }
  }
}

/**
 * Submit: question form — save question, revision, and answers.
 */
function quiz_kit_question_form_submit($form, &$form_state) {
  $node     = $form_state['quiz_node'];
  $question = $form_state['question'];
  $values   = $form_state['values'];
  $now      = REQUEST_TIME;
  $type     = $question ? $question->type : $values['type'];

  if ($question) {
    // New revision of an existing question.
    $vid = db_insert('quiz_kit_question_revision')
      ->fields(array(
        'id'            => $question->id,
        'question_text' => $values['question_text'],
        'created'       => $now,
      ))
      ->execute();

    db_update('quiz_kit_question')
      ->fields(array(
        'vid'     => $vid,
        'changed' => $now,
      ))
      ->condition('id', $question->id)
      ->execute();

    backdrop_set_message(t('Question updated.'));
  }
  else {
    // New question: place it at the end of the current list.
    $next_weight = (int) db_query(
      'SELECT COUNT(*) FROM {quiz_kit_question} WHERE nid = :nid',
      array(':nid' => $node->nid)
    )->fetchField();

    $qid = db_insert('quiz_kit_question')
      ->fields(array(
        'nid'     => $node->nid,
        'vid'     => 0,
        'type'    => $type,
        'weight'  => $next_weight,
        'created' => $now,
        'changed' => $now,
      ))
      ->execute();

    $vid = db_insert('quiz_kit_question_revision')
      ->fields(array(
        'id'            => $qid,
        'question_text' => $values['question_text'],
        'created'       => $now,
      ))
      ->execute();

    db_update('quiz_kit_question')
      ->fields(array('vid' => $vid))
      ->condition('id', $qid)
      ->execute();

    backdrop_set_message(t('Question added.'));
  }

  // Save answers for this revision. Old answers are intentionally kept so
  // that stored results always reflect what respondents actually saw.
  _quiz_kit_save_answers($vid, $type, $values['answers']);

  $form_state['redirect'] = 'node/' . $node->nid . '/questions';
}

// ---------------------------------------------------------------------------
// Answer UI builders
// ---------------------------------------------------------------------------

/**
 * Dispatches to the correct answer UI builder for the given question type.
 */
function _quiz_kit_build_answer_ui(&$form, &$form_state, $type, $existing_answers) {
  switch ($type) {
    case 'mc_single':
    case 'mc_multi':
      _quiz_kit_build_mc_ui($form, $form_state, $type, $existing_answers);
      break;

    case 'truefalse':
      _quiz_kit_build_truefalse_ui($form, $form_state, $existing_answers);
      break;

    case 'short_text':
      _quiz_kit_build_short_text_ui($form, $form_state, $existing_answers);
      break;
  }
}

/**
 * Answer UI for multiple-choice question types (single and multi).
 *
 * Renders one text field per answer row. For mc_single a radio group below
 * the rows selects which answer is correct. For mc_multi each row has an
 * inline "Correct" checkbox.
 */
function _quiz_kit_build_mc_ui(&$form, &$form_state, $type, $existing_answers) {
  $count = $form_state['answer_count'];

  $form['answers']['rows'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('quiz-kit-answer-rows')),
  );

  for ($i = 0; $i < $count; $i++) {
    $existing = isset($existing_answers[$i]) ? $existing_answers[$i] : NULL;

    $form['answers']['rows'][$i] = array(
      '#type' => 'container',
      '#attributes' => array('class' => array('quiz-kit-answer-row')),
    );

    $form['answers']['rows'][$i]['text'] = array(
      '#type'          => 'textfield',
      '#title'         => t('Answer @n', array('@n' => $i + 1)),
      '#default_value' => $existing ? $existing->answer_text : '',
      // Only the first two rows are required; extras may be left blank.
      '#required'      => ($i < 2),
    );

    if ($type === 'mc_multi') {
      $form['answers']['rows'][$i]['is_correct'] = array(
        '#type'          => 'checkbox',
        '#title'         => t('Correct'),
        '#default_value' => $existing ? (int) $existing->is_correct : 0,
      );
    }
  }

  // For mc_single, a single radio group selects the correct answer.
  if ($type === 'mc_single') {
    $correct_index = 0;
    foreach ($existing_answers as $i => $a) {
      if ($a->is_correct) {
        $correct_index = $i;
        break;
      }
    }
    $options = array();
    for ($i = 0; $i < $count; $i++) {
      $options[$i] = t('Answer @n', array('@n' => $i + 1));
    }
    $form['answers']['correct_single'] = array(
      '#type'          => 'radios',
      '#title'         => t('Correct answer'),
      '#options'       => $options,
      '#default_value' => $correct_index,
      '#required'      => TRUE,
    );
  }

  // "Add another answer" — AJAX rebuild that increments the row count.
  $form['answers']['add_answer'] = array(
    '#type'                    => 'submit',
    '#value'                   => t('Add another answer'),
    '#submit'                  => array('quiz_kit_add_answer_submit'),
    '#limit_validation_errors' => array(),
    '#ajax'                    => array(
      'callback' => 'quiz_kit_answers_ajax_callback',
      'wrapper'  => 'quiz-kit-answers-wrapper',
      'effect'   => 'fade',
    ),
  );
}

/**
 * Answer UI for True/False questions.
 *
 * No answer text fields are needed — the two options are fixed. The admin
 * simply picks which one is correct.
 */
function _quiz_kit_build_truefalse_ui(&$form, &$form_state, $existing_answers) {
  $correct = 'true';
  foreach ($existing_answers as $a) {
    if ($a->is_correct && strtolower($a->answer_text) === 'false') {
      $correct = 'false';
      break;
    }
  }

  $form['answers']['correct_tf'] = array(
    '#type'          => 'radios',
    '#title'         => t('Correct answer'),
    '#options'       => array('true' => t('True'), 'false' => t('False')),
    '#default_value' => $correct,
    '#required'      => TRUE,
  );
}

/**
 * Answer UI for short text (exact match) questions.
 */
function _quiz_kit_build_short_text_ui(&$form, &$form_state, $existing_answers) {
  $correct_text = $existing_answers ? reset($existing_answers)->answer_text : '';

  $form['answers']['correct_text'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Correct answer'),
    '#default_value' => $correct_text,
    '#required'      => TRUE,
    '#description'   => t('Responses are matched case-insensitively after trimming whitespace.'),
  );
}

// ---------------------------------------------------------------------------
// Answer UI AJAX handlers
// ---------------------------------------------------------------------------

/**
 * AJAX callback: returns the answers container after a type change or
 * "Add another answer" click.
 */
function quiz_kit_answers_ajax_callback($form, &$form_state) {
  return $form['answers'];
}

/**
 * Submit handler for the "Add another answer" button.
 */
function quiz_kit_add_answer_submit($form, &$form_state) {
  $form_state['answer_count']++;
  $form_state['rebuild'] = TRUE;
}

// ---------------------------------------------------------------------------
// Answer save helper
// ---------------------------------------------------------------------------

/**
 * Saves answer records for a given question revision.
 *
 * Old answers attached to previous revisions are never deleted — they remain
 * linked to any stored quiz results that reference those revisions.
 *
 * @param int    $vid     The question revision ID to attach answers to.
 * @param string $type    The question type.
 * @param array  $answers The 'answers' subtree from $form_state['values'].
 */
function _quiz_kit_save_answers($vid, $type, $answers) {
  switch ($type) {
    case 'mc_single':
      $correct_index = (int) $answers['correct_single'];
      foreach ($answers['rows'] as $i => $row) {
        if (empty($row['text'])) {
          continue;
        }
        db_insert('quiz_kit_answer')
          ->fields(array(
            'question_vid' => $vid,
            'answer_text'  => $row['text'],
            'is_correct'   => ($i === $correct_index) ? 1 : 0,
            'weight'       => $i,
          ))
          ->execute();
      }
      break;

    case 'mc_multi':
      foreach ($answers['rows'] as $i => $row) {
        if (empty($row['text'])) {
          continue;
        }
        db_insert('quiz_kit_answer')
          ->fields(array(
            'question_vid' => $vid,
            'answer_text'  => $row['text'],
            'is_correct'   => !empty($row['is_correct']) ? 1 : 0,
            'weight'       => $i,
          ))
          ->execute();
      }
      break;

    case 'truefalse':
      $correct_tf = $answers['correct_tf'];
      db_insert('quiz_kit_answer')
        ->fields(array(
          'question_vid' => $vid,
          'answer_text'  => 'True',
          'is_correct'   => ($correct_tf === 'true') ? 1 : 0,
          'weight'       => 0,
        ))
        ->execute();
      db_insert('quiz_kit_answer')
        ->fields(array(
          'question_vid' => $vid,
          'answer_text'  => 'False',
          'is_correct'   => ($correct_tf === 'false') ? 1 : 0,
          'weight'       => 1,
        ))
        ->execute();
      break;

    case 'short_text':
      db_insert('quiz_kit_answer')
        ->fields(array(
          'question_vid' => $vid,
          'answer_text'  => $answers['correct_text'],
          'is_correct'   => 1,
          'weight'       => 0,
        ))
        ->execute();
      break;
  }
}

// ---------------------------------------------------------------------------
// Question delete form
// ---------------------------------------------------------------------------

/**
 * Form: confirm question deletion.
 */
function quiz_kit_question_delete_form($form, &$form_state, $node, $question) {
  $form_state['quiz_node'] = $node;
  $form_state['question']  = $question;

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
  $node     = $form_state['quiz_node'];

  $vids = db_query(
    'SELECT vid FROM {quiz_kit_question_revision} WHERE id = :id',
    array(':id' => $question->id)
  )->fetchCol();

  if ($vids) {
    db_delete('quiz_kit_answer')
      ->condition('question_vid', $vids, 'IN')
      ->execute();
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
 * Returns human-readable labels for all question types.
 */
function quiz_kit_question_type_options() {
  return array(
    'mc_single'  => t('Multiple choice — single answer'),
    'mc_multi'   => t('Multiple choice — multiple answers'),
    'truefalse'  => t('True / False'),
    'short_text' => t('Short text (exact match)'),
  );
}

/**
 * Returns the question text for the current revision of a question.
 */
function _quiz_kit_current_question_text($question) {
  return db_query(
    'SELECT question_text FROM {quiz_kit_question_revision} WHERE vid = :vid',
    array(':vid' => $question->vid)
  )->fetchField();
}

/**
 * Returns answer records for a given question revision, ordered by weight.
 */
function _quiz_kit_load_answers_for_vid($vid) {
  return db_query(
    'SELECT * FROM {quiz_kit_answer} WHERE question_vid = :vid ORDER BY weight, id',
    array(':vid' => $vid)
  )->fetchAll();
}
