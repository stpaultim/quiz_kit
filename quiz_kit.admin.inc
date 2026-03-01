<?php
/**
 * @file
 * Admin pages: global settings, section management, question management UI.
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

  $form['quiz_kit_default_results_per_page'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Results per page'),
    '#default_value' => config_get('quiz_kit.settings', 'default_results_per_page') ?: 25,
    '#size'          => 5,
    '#description'   => t('Number of results to show per page on the Results tab.'),
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
  config_set('quiz_kit.settings', 'default_results_per_page', (int) $form_state['values']['quiz_kit_default_results_per_page']);
  config_set('quiz_kit.settings', 'default_answer_count', (int) $form_state['values']['quiz_kit_default_answer_count']);
  backdrop_set_message(t('Quiz Kit settings saved.'));
}

// ---------------------------------------------------------------------------
// Section list / ordering form
// ---------------------------------------------------------------------------

/**
 * Form: list sections for a quiz with tabledrag reordering.
 */
function quiz_kit_sections_form($form, &$form_state, $node) {
  $form_state['quiz_node'] = $node;

  $sections = db_query(
    'SELECT * FROM {quiz_kit_section} WHERE nid = :nid ORDER BY weight, id',
    array(':nid' => $node->nid)
  )->fetchAll();

  if (empty($sections)) {
    $form['empty'] = array(
      '#markup' => t('No sections yet. <a href="@url">Add the first section.</a>', array(
        '@url' => url('node/' . $node->nid . '/sections/add'),
      )),
    );
    return $form;
  }

  $delta = count($sections);

  $form['sections'] = array(
    '#tree'  => TRUE,
    '#theme' => 'quiz_kit_sections_table',
    '#node'  => $node,
  );

  foreach ($sections as $s) {
    $form['sections'][$s->id] = array(
      'title' => array(
        '#markup' => check_plain($s->title),
      ),
      'page_break' => array(
        '#markup' => $s->page_break ? t('Yes') : t('No'),
      ),
      'weight' => array(
        '#type'          => 'weight',
        '#title'         => t('Weight'),
        '#title_display' => 'invisible',
        '#default_value' => $s->weight,
        '#delta'         => $delta,
        '#attributes'    => array('class' => array('quiz-kit-section-weight')),
      ),
      'ops' => array(
        '#markup' => implode(' | ', array(
          l(t('Edit'), 'node/' . $node->nid . '/sections/' . $s->id . '/edit'),
          l(t('Delete'), 'node/' . $node->nid . '/sections/' . $s->id . '/delete'),
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
 * Submit: save the reordered section weights.
 */
function quiz_kit_sections_form_submit($form, &$form_state) {
  foreach ($form_state['values']['sections'] as $sid => $values) {
    db_update('quiz_kit_section')
      ->fields(array('weight' => (int) $values['weight']))
      ->condition('id', $sid)
      ->execute();
  }
  backdrop_set_message(t('Section order saved.'));
}

// ---------------------------------------------------------------------------
// Section add/edit form
// ---------------------------------------------------------------------------

/**
 * Form: add or edit a section.
 *
 * @param object      $node     The quiz node.
 * @param object|null $section  Existing section for edits; NULL for new.
 */
function quiz_kit_section_form($form, &$form_state, $node, $section = NULL) {
  $form_state['quiz_node'] = $node;
  $form_state['section']   = $section;

  $form['title'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Section title'),
    '#default_value' => $section ? $section->title : '',
    '#required'      => TRUE,
    '#maxlength'     => 255,
  );

  $form['instructions'] = array(
    '#type'          => 'textarea',
    '#title'         => t('Instructions'),
    '#default_value' => $section ? $section->instructions : '',
    '#required'      => FALSE,
    '#rows'          => 4,
    '#description'   => t('Optional text shown to quiz-takers at the top of this section.'),
  );

  $form['page_break'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Start a new page before this section'),
    '#default_value' => $section ? (int) $section->page_break : 0,
    '#description'   => t('When checked, quiz-takers will see a Next button and this section will appear on its own page. Has no effect on the first section that contains questions.'),
  );

  $form['validate_on_next'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Require answers before advancing'),
    '#default_value' => $section ? (int) $section->validate_on_next : 0,
    '#description'   => t('When checked, quiz-takers must answer all required questions on this page before the Next button will advance them.'),
  );

  $form['lock_on_advance'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Prevent going back once advanced'),
    '#default_value' => $section ? (int) $section->lock_on_advance : 0,
    '#description'   => t('When checked, quiz-takers cannot use the Back button to return to this page or any earlier page once they have moved past it.'),
  );

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $section ? t('Save section') : t('Add section'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'node/' . $node->nid . '/sections'),
  );

  return $form;
}

/**
 * Submit: section add/edit form.
 */
function quiz_kit_section_form_submit($form, &$form_state) {
  $node    = $form_state['quiz_node'];
  $section = $form_state['section'];
  $values  = $form_state['values'];

  if ($section) {
    db_update('quiz_kit_section')
      ->fields(array(
        'title'            => $values['title'],
        'instructions'     => $values['instructions'],
        'page_break'       => (int) $values['page_break'],
        'validate_on_next' => (int) $values['validate_on_next'],
        'lock_on_advance'  => (int) $values['lock_on_advance'],
      ))
      ->condition('id', $section->id)
      ->execute();
    backdrop_set_message(t('Section updated.'));
  }
  else {
    $next_weight = (int) db_query(
      'SELECT COUNT(*) FROM {quiz_kit_section} WHERE nid = :nid',
      array(':nid' => $node->nid)
    )->fetchField();

    db_insert('quiz_kit_section')
      ->fields(array(
        'nid'              => $node->nid,
        'title'            => $values['title'],
        'instructions'     => $values['instructions'],
        'weight'           => $next_weight,
        'page_break'       => (int) $values['page_break'],
        'validate_on_next' => (int) $values['validate_on_next'],
        'lock_on_advance'  => (int) $values['lock_on_advance'],
      ))
      ->execute();
    backdrop_set_message(t('Section added.'));
  }

  $form_state['redirect'] = 'node/' . $node->nid . '/sections';
}

// ---------------------------------------------------------------------------
// Section delete form
// ---------------------------------------------------------------------------

/**
 * Form: confirm section deletion.
 */
function quiz_kit_section_delete_form($form, &$form_state, $node, $section) {
  $form_state['quiz_node'] = $node;
  $form_state['section']   = $section;

  return confirm_form(
    $form,
    t('Are you sure you want to delete the section %title?', array('%title' => $section->title)),
    'node/' . $node->nid . '/sections',
    t('All questions currently assigned to this section will be moved to "no section". Section score history will also be removed.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit: section delete confirm form.
 */
function quiz_kit_section_delete_form_submit($form, &$form_state) {
  $section = $form_state['section'];
  $node    = $form_state['quiz_node'];

  // Unassign questions from this section.
  db_update('quiz_kit_quiz_question')
    ->fields(array('section_id' => 0))
    ->condition('section_id', $section->id)
    ->execute();

  // Delete per-section result history.
  db_delete('quiz_kit_result_section')
    ->condition('section_id', $section->id)
    ->execute();

  // Delete the section record.
  db_delete('quiz_kit_section')
    ->condition('id', $section->id)
    ->execute();

  backdrop_set_message(t('Section %title deleted.', array('%title' => $section->title)));
  $form_state['redirect'] = 'node/' . $node->nid . '/sections';
}

// ---------------------------------------------------------------------------
// Question list / ordering form
// ---------------------------------------------------------------------------

/**
 * Form: list questions for a quiz grouped by section with tabledrag reordering.
 *
 * Questions are displayed under their assigned section heading. Drag handles
 * allow within-section reordering. Section assignment is changed on the
 * question add/edit form, not here.
 */
function quiz_kit_questions_form($form, &$form_state, $node) {
  $form_state['quiz_node'] = $node;

  // Load sections in display order.
  $sections = db_query(
    'SELECT * FROM {quiz_kit_section} WHERE nid = :nid ORDER BY weight, id',
    array(':nid' => $node->nid)
  )->fetchAllAssoc('id');

  // Load questions ordered by section weight first, then question weight,
  // so the initial table order matches the quiz display order.
  $questions = db_query(
    'SELECT qq.id AS qq_id, qq.weight, qq.section_id, q.id, q.type, r.question_text
     FROM {quiz_kit_quiz_question} qq
     JOIN {quiz_kit_question} q ON q.id = qq.question_id
     JOIN {quiz_kit_question_revision} r ON r.vid = q.vid
     LEFT JOIN {quiz_kit_section} s ON s.id = qq.section_id
     WHERE qq.nid = :nid
     ORDER BY
       CASE WHEN qq.section_id = 0 THEN -1 ELSE COALESCE(s.weight, 0) END,
       COALESCE(s.id, 0),
       qq.weight,
       q.id',
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
  $delta       = count($questions);

  $form['questions'] = array(
    '#tree'  => TRUE,
    '#theme' => 'quiz_kit_questions_table',
    '#node'  => $node,
  );

  // Walk questions in section order, injecting a header row each time the
  // section changes. Section header entries are plain containers with no
  // form inputs so they never appear in $form_state['values'].
  $last_section_id = -1;

  foreach ($questions as $q) {
    $sid = (int) $q->section_id;

    if ($sid !== $last_section_id) {
      if ($sid === 0) {
        $label = t('(No section)');
      }
      elseif (isset($sections[$sid])) {
        $sec   = $sections[$sid];
        $label = check_plain($sec->title);
        if ($sec->page_break) {
          $label .= ' — ' . t('page break');
        }
      }
      else {
        $label = t('Unknown section');
      }

      $form['questions']['section_header_' . $sid] = array(
        '#is_section_header' => TRUE,
        '#section_label'     => $label,
      );
      $last_section_id = $sid;
    }

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
 *
 * Section assignment is not changed here — edit the question to reassign it.
 */
function quiz_kit_questions_form_submit($form, &$form_state) {
  $node = $form_state['quiz_node'];
  foreach ($form_state['values']['questions'] as $qid => $values) {
    db_update('quiz_kit_quiz_question')
      ->fields(array('weight' => (int) $values['weight']))
      ->condition('question_id', $qid)
      ->condition('nid', $node->nid)
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

  // Reset answer slots if the type changed since the last build.
  if (isset($form_state['prev_question_type']) && $form_state['prev_question_type'] !== $type) {
    unset($form_state['answer_slots']);
  }
  $form_state['prev_question_type'] = $type;

  // Initialise answer slots once per type. Each slot is an integer ID that
  // remains stable across AJAX rebuilds even when rows are removed.
  if (!isset($form_state['answer_slots'])) {
    if ($existing_answers) {
      $form_state['answer_slots'] = range(0, count($existing_answers) - 1);
    }
    else {
      $default = in_array($type, array('mc_single', 'mc_any', 'mc_multi'))
        ? (int) (config_get('quiz_kit.settings', 'default_answer_count') ?: 3)
        : 1;
      $form_state['answer_slots'] = range(0, $default - 1);
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

  // --- Section assignment ---
  $section_options = array(0 => t('— None —'));
  $sections = db_query(
    'SELECT id, title FROM {quiz_kit_section} WHERE nid = :nid ORDER BY weight, id',
    array(':nid' => $node->nid)
  )->fetchAllKeyed();
  $section_options += $sections;

  if ($question) {
    $qq = db_query(
      'SELECT * FROM {quiz_kit_quiz_question} WHERE question_id = :qid AND nid = :nid',
      array(':qid' => $question->id, ':nid' => $node->nid)
    )->fetchObject();
    $section_id_default = $qq ? (int) $qq->section_id : 0;
  }
  else {
    $section_id_default = 0;
  }

  $form['section_id'] = array(
    '#type'          => 'select',
    '#title'         => t('Section'),
    '#options'       => $section_options,
    '#default_value' => $section_id_default,
    '#description'   => t('Assign this question to a section. Sections can be managed on the <a href="@url">Sections tab</a>.', array(
      '@url' => url('node/' . $node->nid . '/sections'),
    )),
  );

  // --- Answer layout (only relevant for types that have multiple choices) ---
  if (in_array($type, array('mc_single', 'mc_any', 'mc_multi', 'truefalse'))) {
    $layout_default = 'horizontal';
    if ($question && isset($question->answer_layout)) {
      $layout_default = $question->answer_layout;
    }
    $form['answer_layout'] = array(
      '#type'          => 'radios',
      '#title'         => t('Answer display'),
      '#options'       => array('horizontal' => t('Horizontal'), 'vertical' => t('Vertical')),
      '#default_value' => $layout_default,
      '#description'   => t('How answer choices are displayed to quiz-takers.'),
    );
  }

  // --- Question text ---
  $form['question_text'] = array(
    '#type' => 'textarea',
    '#title' => t('Question'),
    '#default_value' => $question ? _quiz_kit_current_question_text($question) : '',
    '#required' => TRUE,
    '#rows' => 3,
  );

  // --- Include in score ---
  $scored_default = ($type === 'long_text') ? 0 : 1;
  if ($question && isset($question->scored)) {
    $scored_default = (int) $question->scored;
  }
  $form['scored'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Include in score'),
    '#default_value' => $scored_default,
    '#description'   => t('Uncheck for metadata questions (e.g. Name, Class) whose responses should be recorded but not counted toward the score.'),
  );

  $form['#attached']['css'][] = backdrop_get_path('module', 'quiz_kit') . '/css/quiz_kit.admin.css';

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
    '#value' => t('Save question'),
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

  if (in_array($type, array('mc_single', 'mc_any', 'mc_multi'))) {
    $filled = 0;
    foreach ($form_state['values']['answers']['rows'] as $row) {
      if (!empty($row['text'])) {
        $filled++;
      }
    }
    if ($filled < 2) {
      form_set_error('answers', t('Please provide at least two answer options.'));
    }
    if ($type === 'mc_single') {
      if (!isset($form_state['values']['answers']['correct_single'])) {
        form_set_error('answers', t('Please select the correct answer.'));
      }
    }
    if ($type === 'mc_multi' || $type === 'mc_any') {
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
  $node      = $form_state['quiz_node'];
  $question  = $form_state['question'];
  $values    = $form_state['values'];
  $now       = REQUEST_TIME;
  $type      = $question ? $question->type : $values['type'];
  $section_id    = isset($values['section_id'])    ? (int) $values['section_id']   : 0;
  $scored        = isset($values['scored'])        ? (int) $values['scored']       : 1;
  $answer_layout = isset($values['answer_layout']) ? $values['answer_layout']      : 'horizontal';
  if (!in_array($answer_layout, array('horizontal', 'vertical'))) {
    $answer_layout = 'horizontal';
  }

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
      ->fields(array('vid' => $vid, 'changed' => $now, 'answer_layout' => $answer_layout, 'scored' => $scored))
      ->condition('id', $question->id)
      ->execute();

    db_update('quiz_kit_quiz_question')
      ->fields(array('section_id' => $section_id))
      ->condition('question_id', $question->id)
      ->condition('nid', $node->nid)
      ->execute();

    backdrop_set_message(t('Question updated.'));
  }
  else {
    // New question: insert into bank, then create the junction record.
    $qid = db_insert('quiz_kit_question')
      ->fields(array(
        'vid'           => 0,
        'type'          => $type,
        'created'       => $now,
        'changed'       => $now,
        'answer_layout' => $answer_layout,
        'scored'        => $scored,
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

    // Place at the end of this quiz's question list.
    $next_weight = (int) db_query(
      'SELECT COUNT(*) FROM {quiz_kit_quiz_question} WHERE nid = :nid',
      array(':nid' => $node->nid)
    )->fetchField();

    db_insert('quiz_kit_quiz_question')
      ->fields(array(
        'nid'        => $node->nid,
        'question_id' => $qid,
        'section_id' => $section_id,
        'weight'     => $next_weight,
      ))
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
    case 'mc_any':
    case 'mc_multi':
      _quiz_kit_build_mc_ui($form, $form_state, $type, $existing_answers);
      break;

    case 'truefalse':
      _quiz_kit_build_truefalse_ui($form, $form_state, $existing_answers);
      break;

    case 'short_text':
      _quiz_kit_build_short_text_ui($form, $form_state, $existing_answers);
      break;

    case 'long_text':
      _quiz_kit_build_long_text_ui($form, $form_state);
      break;
  }
}

/**
 * Answer UI for multiple-choice question types (single, any, and multi).
 *
 * Each answer row contains the correct-answer indicator (radio or checkbox),
 * the answer text field, and an optional Remove button — all on one line.
 *
 * For mc_single the radio buttons share #parents so they act as a group and
 * their selected value is stored at $form_state['values']['answers']['correct_single'].
 */
function _quiz_kit_build_mc_ui(&$form, &$form_state, $type, $existing_answers) {
  $slots      = $form_state['answer_slots'];
  $slot_count = count($slots);
  $existing   = array_values($existing_answers);

  // For mc_single, determine which slot is currently the correct answer.
  $correct_slot = NULL;
  if ($type === 'mc_single') {
    $correct_slot = $slots[0];
    foreach ($existing as $position => $a) {
      if ($a->is_correct && isset($slots[$position])) {
        $correct_slot = $slots[$position];
        break;
      }
    }
  }

  if ($type === 'mc_single') {
    $hint = t('Select the radio button next to the correct answer. At least 2 answers are required.');
  }
  else {
    $hint = t('Check the box next to each correct answer. At least 2 answers are required.');
  }
  $form['answers']['hint'] = array(
    '#markup' => '<p class="description">' . $hint . '</p>',
    '#weight' => -1,
  );

  $form['answers']['rows'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('quiz-kit-answer-rows')),
  );

  foreach ($slots as $position => $slot) {
    $existing_answer = isset($existing[$position]) ? $existing[$position] : NULL;

    $form['answers']['rows'][$slot] = array(
      '#type'       => 'container',
      '#attributes' => array('class' => array('quiz-kit-answer-row')),
    );

    // Correct-answer indicator — first in the row.
    if ($type === 'mc_single') {
      // All per-row radios share the same #parents so they behave as one group.
      // The selected slot ID ends up at $values['answers']['correct_single'].
      $form['answers']['rows'][$slot]['is_correct'] = array(
        '#type'          => 'radio',
        '#title'         => t('Correct'),
        '#title_display' => 'invisible',
        '#parents'       => array('answers', 'correct_single'),
        '#return_value'  => $slot,
        '#default_value' => $correct_slot,
      );
    }
    elseif ($type === 'mc_multi' || $type === 'mc_any') {
      $form['answers']['rows'][$slot]['is_correct'] = array(
        '#type'          => 'checkbox',
        '#title'         => t('Correct'),
        '#title_display' => 'invisible',
        '#default_value' => $existing_answer ? (int) $existing_answer->is_correct : 0,
      );
    }

    // Answer text field — fills remaining space via CSS.
    $form['answers']['rows'][$slot]['text'] = array(
      '#type'          => 'textfield',
      '#title'         => t('Answer @n', array('@n' => $position + 1)),
      '#default_value' => $existing_answer ? $existing_answer->answer_text : '',
      '#required'      => ($position < 2),
    );

    // Remove button — only shown when more than the minimum two rows exist.
    if ($slot_count > 2) {
      $form['answers']['rows'][$slot]['remove'] = array(
        '#type'                    => 'submit',
        '#value'                   => t('Remove'),
        '#name'                    => 'remove_answer_' . $slot,
        '#submit'                  => array('quiz_kit_remove_answer_submit'),
        '#limit_validation_errors' => array(),
        '#attributes'              => array('class' => array('quiz-kit-remove-answer')),
        '#ajax'                    => array(
          'callback' => 'quiz_kit_answers_ajax_callback',
          'wrapper'  => 'quiz-kit-answers-wrapper',
          'effect'   => 'fade',
        ),
      );
    }
  }

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

/**
 * Answer UI for long text (open response) questions.
 *
 * No correct answer is defined — respondents see a textarea and their
 * response is recorded but not graded.
 */
function _quiz_kit_build_long_text_ui(&$form, &$form_state) {
  $form['answers']['note'] = array(
    '#markup' => '<p class="description">' . t('Respondents will see a text area for their open-ended response. No correct answer is defined and this question does not affect the score.') . '</p>',
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
  $slots = $form_state['answer_slots'];
  $form_state['answer_slots'][] = empty($slots) ? 0 : max($slots) + 1;
  $form_state['rebuild'] = TRUE;
}

/**
 * Submit handler for per-row "Remove" buttons.
 *
 * Reads the slot ID from the triggering element's #name
 * (format: "remove_answer_{slot}") and removes it from the active slots.
 */
function quiz_kit_remove_answer_submit($form, &$form_state) {
  $name = $form_state['triggering_element']['#name'];
  $slot = (int) str_replace('remove_answer_', '', $name);
  $form_state['answer_slots'] = array_values(
    array_filter($form_state['answer_slots'], function($s) use ($slot) {
      return $s !== $slot;
    })
  );
  $form_state['rebuild'] = TRUE;
}

// ---------------------------------------------------------------------------
// Answer save helper
// ---------------------------------------------------------------------------

/**
 * Saves answer records for a given question revision.
 */
function _quiz_kit_save_answers($vid, $type, $answers) {
  switch ($type) {
    case 'mc_single':
      $correct_slot = (int) $answers['correct_single'];
      $weight = 0;
      foreach ($answers['rows'] as $slot => $row) {
        if (empty($row['text'])) {
          continue;
        }
        db_insert('quiz_kit_answer')
          ->fields(array(
            'question_vid' => $vid,
            'answer_text'  => $row['text'],
            'is_correct'   => ($slot === $correct_slot) ? 1 : 0,
            'weight'       => $weight++,
          ))
          ->execute();
      }
      break;

    case 'mc_any':
    case 'mc_multi':
      $weight = 0;
      foreach ($answers['rows'] as $slot => $row) {
        if (empty($row['text'])) {
          continue;
        }
        db_insert('quiz_kit_answer')
          ->fields(array(
            'question_vid' => $vid,
            'answer_text'  => $row['text'],
            'is_correct'   => !empty($row['is_correct']) ? 1 : 0,
            'weight'       => $weight++,
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

    case 'long_text':
      // No answer records — open response, not graded.
      break;
  }
}

// ---------------------------------------------------------------------------
// Add existing question form
// ---------------------------------------------------------------------------

/**
 * Form: add an existing question from the bank to this quiz.
 */
function quiz_kit_question_add_existing_form($form, &$form_state, $node) {
  $form_state['quiz_node'] = $node;

  // Questions that exist in the bank but are not yet in this quiz.
  $available = db_query(
    'SELECT q.id, q.type, r.question_text
     FROM {quiz_kit_question} q
     JOIN {quiz_kit_question_revision} r ON r.vid = q.vid
     WHERE q.id NOT IN (
       SELECT question_id FROM {quiz_kit_quiz_question} WHERE nid = :nid
     )
     ORDER BY r.question_text',
    array(':nid' => $node->nid)
  )->fetchAll();

  if (empty($available)) {
    $form['empty'] = array(
      '#markup' => t('No questions available. All questions in the bank are already in this quiz.'),
    );
    $form['cancel'] = array(
      '#markup' => '<p>' . l(t('Back to questions'), 'node/' . $node->nid . '/questions') . '</p>',
    );
    return $form;
  }

  $type_labels = quiz_kit_question_type_options();
  $options = array();
  foreach ($available as $q) {
    $type_label = isset($type_labels[$q->type]) ? $type_labels[$q->type] : $q->type;
    $options[$q->id] = $q->question_text . ' [' . $type_label . ']';
  }

  $form['question_id'] = array(
    '#type'     => 'select',
    '#title'    => t('Question'),
    '#options'  => $options,
    '#required' => TRUE,
  );

  $section_options = array(0 => t('— None —'));
  $sections = db_query(
    'SELECT id, title FROM {quiz_kit_section} WHERE nid = :nid ORDER BY weight, id',
    array(':nid' => $node->nid)
  )->fetchAllKeyed();
  $section_options += $sections;

  $form['section_id'] = array(
    '#type'        => 'select',
    '#title'       => t('Section'),
    '#options'     => $section_options,
    '#description' => t('Optionally assign this question to a section.'),
  );

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Add to quiz'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'node/' . $node->nid . '/questions'),
  );

  return $form;
}

/**
 * Submit: add existing question form.
 */
function quiz_kit_question_add_existing_form_submit($form, &$form_state) {
  $node       = $form_state['quiz_node'];
  $qid        = (int) $form_state['values']['question_id'];
  $section_id = (int) $form_state['values']['section_id'];

  $next_weight = (int) db_query(
    'SELECT COUNT(*) FROM {quiz_kit_quiz_question} WHERE nid = :nid',
    array(':nid' => $node->nid)
  )->fetchField();

  db_insert('quiz_kit_quiz_question')
    ->fields(array(
      'nid'         => $node->nid,
      'question_id' => $qid,
      'section_id'  => $section_id,
      'weight'      => $next_weight,
    ))
    ->execute();

  backdrop_set_message(t('Question added to quiz.'));
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
  db_delete('quiz_kit_quiz_question')->condition('question_id', $question->id)->execute();
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
    'mc_single'  => t('Multiple choice — one correct answer'),
    'mc_any'     => t('Multiple choice — any one correct answer (multiple may be correct)'),
    'mc_multi'   => t('Multiple choice — all correct answers required'),
    'truefalse'  => t('True / False'),
    'short_text' => t('Short text (exact match)'),
    'long_text'  => t('Long text (open response, not scored)'),
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

// ---------------------------------------------------------------------------
// Question bank page
// ---------------------------------------------------------------------------

/**
 * Page callback: list all questions across all quizzes.
 */
function quiz_kit_question_bank_page() {
  $per_page = (int) (config_get('quiz_kit.settings', 'default_results_per_page') ?: 25);

  $query = db_select('quiz_kit_question', 'q')
    ->extend('PagerDefault')
    ->limit($per_page);
  $query->join('quiz_kit_question_revision', 'r', 'r.vid = q.vid');
  $query->join('quiz_kit_quiz_question', 'qq', 'qq.question_id = q.id');
  $query->join('node', 'n', 'n.nid = qq.nid');
  $query->fields('q', array('id', 'type'));
  $query->addField('r', 'question_text');
  $query->addField('n', 'title', 'quiz_title');
  $query->addField('n', 'nid');
  $query->orderBy('n.title');
  $query->orderBy('qq.weight');
  $questions = $query->execute()->fetchAll();

  if (empty($questions)) {
    return t('No questions exist yet.');
  }

  $type_labels = quiz_kit_question_type_options();
  $header = array(t('Quiz'), t('Type'), t('Question'));
  $rows = array();
  foreach ($questions as $q) {
    $rows[] = array(
      l(check_plain($q->quiz_title), 'node/' . $q->nid . '/questions'),
      isset($type_labels[$q->type]) ? $type_labels[$q->type] : check_plain($q->type),
      check_plain($q->question_text),
    );
  }

  $output  = theme('table', array('header' => $header, 'rows' => $rows));
  $output .= theme('pager');
  return $output;
}
