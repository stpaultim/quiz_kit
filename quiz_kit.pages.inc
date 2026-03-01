<?php
/**
 * @file
 * User-facing pages: quiz listing, take quiz, results.
 */

/**
 * Page callback: list all published quizzes.
 */
function quiz_kit_list_page() {
  $nodes = db_query(
    "SELECT n.nid, n.title FROM {node} n WHERE n.type = 'quiz_kit_quiz' AND n.status = 1 ORDER BY n.title"
  )->fetchAll();

  if (empty($nodes)) {
    return t('No quizzes are available.');
  }

  $items = array();
  foreach ($nodes as $n) {
    $items[] = l(check_plain($n->title), 'node/' . $n->nid);
  }

  return theme('item_list', array('items' => $items));
}

// ---------------------------------------------------------------------------
// Take quiz
// ---------------------------------------------------------------------------

/**
 * Page callback: take a quiz.
 */
function quiz_kit_take_page($node) {
  global $user;

  // Check attempt limits.
  $max = (int) _quiz_kit_node_setting($node, 'max_attempts', 0);
  if ($max > 0) {
    $attempts = db_query(
      'SELECT COUNT(*) FROM {quiz_kit_result} WHERE nid = :nid AND uid = :uid AND finished > 0',
      array(':nid' => $node->nid, ':uid' => $user->uid)
    )->fetchField();
    if ($attempts >= $max) {
      return t('You have reached the maximum number of attempts for this quiz.');
    }
  }

  // Warn editors previewing an unpublished quiz.
  if (!$node->status && node_access('update', $node)) {
    backdrop_set_message(
      t('This quiz is unpublished. You are previewing it as an editor. <a href="@url">Publish it</a> when you are ready for others to take it.',
        array('@url' => url('node/' . $node->nid . '/edit'))),
      'warning'
    );
  }

  // Load questions.
  $questions = _quiz_kit_load_questions($node->nid);
  if (empty($questions)) {
    return t('This quiz has no questions yet.');
  }

  // Compute pages (groups of questions separated by section page breaks).
  $pages = _quiz_kit_compute_pages($node->nid, $questions);

  return backdrop_get_form('quiz_kit_take_form', $node, $pages);
}

/**
 * Form: quiz-taking form (supports multi-step pagination via sections).
 *
 * $pages is an array of pages; each page is an array of groups:
 *   array('section' => $section_object_or_NULL, 'questions' => [...])
 */
function quiz_kit_take_form($form, &$form_state, $node, $pages) {
  $form_state['quiz_node'] = $node;

  // Initialise persistent storage on the first build only.
  if (!isset($form_state['storage']['result_id'])) {
    global $user;
    $form_state['storage']['result_id'] = db_insert('quiz_kit_result')
      ->fields(array(
        'nid'      => $node->nid,
        'uid'      => $user->uid,
        'score'    => 0,
        'passed'   => 0,
        'started'  => REQUEST_TIME,
        'finished' => 0,
      ))
      ->execute();
    $form_state['storage']['current_page']        = 0;
    $form_state['storage']['accumulated_answers'] = array();
  }

  $current_page = $form_state['storage']['current_page'];
  $total_pages  = count($pages);
  $page_data    = isset($pages[$current_page]) ? $pages[$current_page] : array();

  $form['#tree'] = TRUE;

  // Progress indicator for multi-page quizzes.
  if ($total_pages > 1) {
    $form['progress'] = array(
      '#markup' => '<div class="quiz-kit-progress">'
        . t('Page @current of @total', array('@current' => $current_page + 1, '@total' => $total_pages))
        . '</div>',
      '#weight' => -10,
    );
  }

  $required = (bool) _quiz_kit_node_setting($node, 'require_answers', TRUE);

  // Render section headers and questions for the current page.
  // Both headers and questions go inside $form['questions'] so that weights
  // can interleave them correctly. Top-level section_header elements can't
  // interleave with the questions container — they always sort after it.
  foreach ($page_data as $group_idx => $group) {
    $base_weight = $group_idx * 100;

    if (!empty($group['section'])) {
      $sec    = $group['section'];
      $markup = '<div class="quiz-kit-section-header">'
        . '<h3>' . check_plain($sec->title) . '</h3>';
      if (!empty($sec->instructions)) {
        $markup .= '<div class="quiz-kit-section-instructions">'
          . nl2br(check_plain($sec->instructions))
          . '</div>';
      }
      $markup .= '</div>';
      $form['questions']['section_header_' . $sec->id] = array(
        '#markup' => $markup,
        '#weight' => $base_weight,
      );
    }

    foreach ($group['questions'] as $q_idx => $q) {
      $answers  = _quiz_kit_load_answers($q->vid);
      $element  = _quiz_kit_question_element($q, $answers, $required);
      $element['#weight'] = $base_weight + $q_idx + 1;
      $form['questions'][$q->id] = $element;
    }
  }

  $form['actions']['#type'] = 'actions';
  if ($current_page < $total_pages - 1) {
    $form['actions']['next'] = array(
      '#type'   => 'submit',
      '#value'  => t('Next'),
      '#submit' => array('quiz_kit_take_form_next_submit'),
    );
  }
  else {
    $form['actions']['submit'] = array(
      '#type'  => 'submit',
      '#value' => t('Submit quiz'),
    );
  }

  return $form;
}

/**
 * Validate: quiz-taking form.
 *
 * When the quiz requires all answers, radios and textfields are already
 * covered by #required on the element. mc_multi checkboxes need explicit
 * validation here because an all-unchecked checkboxes group is not considered
 * "empty" by Backdrop's built-in required check.
 */
function quiz_kit_take_form_validate($form, &$form_state) {
  $node = $form_state['quiz_node'];
  if (!(bool) _quiz_kit_node_setting($node, 'require_answers', TRUE)) {
    return;
  }
  if (empty($form_state['values']['questions'])) {
    return;
  }
  foreach ($form_state['values']['questions'] as $qid => $data) {
    if (!isset($data['answer']) || !is_array($data['answer'])) {
      continue;
    }
    // is_array means this is a checkboxes (mc_multi) answer.
    if (empty(array_filter($data['answer']))) {
      form_set_error('questions][' . $qid . '][answer',
        t('Please select at least one answer for this question.'));
    }
  }
}

/**
 * Submit: advance to the next page of the quiz.
 *
 * Accumulates this page's answers into storage and triggers a form rebuild
 * so the next page's questions are displayed.
 */
function quiz_kit_take_form_next_submit($form, &$form_state) {
  // Save answers from the current page.
  if (!empty($form_state['values']['questions'])) {
    foreach ($form_state['values']['questions'] as $qid => $data) {
      $form_state['storage']['accumulated_answers'][$qid] = $data;
    }
  }

  $form_state['storage']['current_page']++;
  $form_state['rebuild'] = TRUE;
}

/**
 * Submit: quiz-taking form final page — grade and save results.
 */
function quiz_kit_take_form_submit($form, &$form_state) {
  $node      = $form_state['quiz_node'];
  $result_id = $form_state['storage']['result_id'];

  // Merge answers accumulated from earlier pages with the final page's answers.
  $responses = $form_state['storage']['accumulated_answers'];
  if (!empty($form_state['values']['questions'])) {
    $responses += $form_state['values']['questions'];
  }

  $all_qs    = _quiz_kit_load_questions($node->nid);
  $total     = 0;
  $correct   = 0;
  $grade_map = array();

  foreach ($all_qs as $q) {
    $given      = isset($responses[$q->id]['answer']) ? $responses[$q->id]['answer'] : NULL;
    $is_correct = _quiz_kit_grade_answer($q, $given);
    $grade_map[$q->id] = $is_correct;
    if ((int) $q->scored) {
      $total++;
      if ($is_correct) {
        $correct++;
      }
    }

    db_insert('quiz_kit_result_answer')
      ->fields(array(
        'result_id'    => $result_id,
        'question_id'  => $q->id,
        'question_vid' => $q->vid,
        'answer_given' => is_array($given) ? serialize($given) : (string) $given,
        'is_correct'   => (int) $is_correct,
      ))
      ->execute();
  }

  $score     = $total > 0 ? round(($correct / $total) * 100, 1) : 0;
  $pass_rate = (int) _quiz_kit_node_setting($node, 'pass_rate', 75);
  $passed    = $score >= $pass_rate ? 1 : 0;

  db_update('quiz_kit_result')
    ->fields(array(
      'score'    => $score,
      'passed'   => $passed,
      'finished' => REQUEST_TIME,
    ))
    ->condition('id', $result_id)
    ->execute();

  // Save per-section scores when the quiz is configured to show them.
  if ((bool) _quiz_kit_node_setting($node, 'show_section_scores', FALSE)) {
    _quiz_kit_save_section_scores($result_id, $all_qs, $grade_map);
  }

  $form_state['redirect'] = 'node/' . $node->nid . '/results/' . $result_id;
}

// ---------------------------------------------------------------------------
// Results pages
// ---------------------------------------------------------------------------

/**
 * Page callback: list all results for a quiz (admin/author view).
 */
function quiz_kit_results_list_page($node) {
  $results = db_query(
    'SELECT r.*, u.name FROM {quiz_kit_result} r
     LEFT JOIN {users} u ON u.uid = r.uid
     WHERE r.nid = :nid AND r.finished > 0
     ORDER BY r.finished DESC',
    array(':nid' => $node->nid)
  )->fetchAll();

  if (empty($results)) {
    return t('No completed attempts yet.');
  }

  $header = array(t('User'), t('Score'), t('Passed'), t('Date'), t('Detail'));
  $rows = array();
  foreach ($results as $r) {
    $rows[] = array(
      check_plain($r->name ?: t('Anonymous')),
      $r->score . '%',
      $r->passed ? t('Yes') : t('No'),
      format_date($r->finished, 'short'),
      l(t('View'), 'node/' . $node->nid . '/results/' . $r->id),
    );
  }

  return theme('table', array('header' => $header, 'rows' => $rows));
}

/**
 * Page callback: single result detail.
 */
function quiz_kit_result_detail_page($node, $result) {
  // Include question type so we can decode answer_given correctly.
  $answers = db_query(
    'SELECT ra.*, r.question_text, q.type, q.scored
     FROM {quiz_kit_result_answer} ra
     JOIN {quiz_kit_question_revision} r ON r.vid = ra.question_vid
     JOIN {quiz_kit_question} q ON q.id = ra.question_id
     WHERE ra.result_id = :rid',
    array(':rid' => $result->id)
  )->fetchAll();

  // Decode each answer_given into a human-readable string.
  // For incorrect answers, also attach the correct answer text.
  foreach ($answers as $a) {
    $a->answer_display = _quiz_kit_decode_answer_given($a->type, $a->answer_given);
    $a->correct_answer_display = $a->is_correct ? NULL : _quiz_kit_correct_answer_display($a->type, $a->question_vid);
  }

  $show_correct = (bool) _quiz_kit_node_setting($node, 'show_answers', TRUE);

  // Load per-section scores if the quiz is configured to show them.
  $show_section_scores = (bool) _quiz_kit_node_setting($node, 'show_section_scores', FALSE);
  $section_scores = array();
  if ($show_section_scores) {
    $section_scores = db_query(
      'SELECT rs.score, s.title FROM {quiz_kit_result_section} rs
       JOIN {quiz_kit_section} s ON s.id = rs.section_id
       WHERE rs.result_id = :rid
       ORDER BY s.weight, s.id',
      array(':rid' => $result->id)
    )->fetchAll();
  }

  return theme('quiz_kit_results', array(
    'node'                => $node,
    'result'              => $result,
    'answers'             => $answers,
    'show_correct'        => $show_correct,
    'section_scores'      => $section_scores,
    'show_section_scores' => $show_section_scores,
  ));
}

/**
 * Decodes a stored answer_given value into human-readable text.
 *
 * @param string $type         The question type (mc_single, mc_multi, etc.).
 * @param string $answer_given The raw stored value.
 *
 * @return string
 */
function _quiz_kit_decode_answer_given($type, $answer_given) {
  switch ($type) {
    case 'mc_single':
    case 'mc_any':
    case 'truefalse':
      if (is_numeric($answer_given) && (int) $answer_given > 0) {
        $text = db_query(
          'SELECT answer_text FROM {quiz_kit_answer} WHERE id = :id',
          array(':id' => $answer_given)
        )->fetchField();
        return ($text !== FALSE) ? $text : $answer_given;
      }
      return t('(no answer)');

    case 'mc_multi':
      $data = @unserialize($answer_given);
      if (!is_array($data)) {
        return $answer_given;
      }
      $checked_ids = array_keys(array_filter($data));
      if (empty($checked_ids)) {
        return t('(no answer)');
      }
      $texts = db_query(
        'SELECT answer_text FROM {quiz_kit_answer} WHERE id IN (:ids) ORDER BY weight',
        array(':ids' => $checked_ids)
      )->fetchCol();
      return implode(', ', $texts);

    case 'short_text':
    case 'long_text':
    default:
      return ($answer_given !== '') ? $answer_given : t('(no answer)');
  }
}

/**
 * Page callback: a user's own result history.
 */
function quiz_kit_user_results_page($account) {
  $results = db_query(
    "SELECT r.*, n.title FROM {quiz_kit_result} r
     JOIN {node} n ON n.nid = r.nid
     WHERE r.uid = :uid AND r.finished > 0
     ORDER BY r.finished DESC",
    array(':uid' => $account->uid)
  )->fetchAll();

  if (empty($results)) {
    return t('No quiz results yet.');
  }

  $header = array(t('Quiz'), t('Score'), t('Passed'), t('Date'), t('Detail'));
  $rows = array();
  foreach ($results as $r) {
    $rows[] = array(
      l(check_plain($r->title), 'node/' . $r->nid),
      $r->score . '%',
      $r->passed ? t('Yes') : t('No'),
      format_date($r->finished, 'short'),
      l(t('View'), 'node/' . $r->nid . '/results/' . $r->id),
    );
  }

  return theme('table', array('header' => $header, 'rows' => $rows));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Loads questions for a quiz node, in weight/id order.
 */
function _quiz_kit_load_questions($nid) {
  return db_query(
    'SELECT q.id, q.vid, q.type, q.answer_layout, q.scored, qq.section_id, qq.weight
     FROM {quiz_kit_question} q
     JOIN {quiz_kit_quiz_question} qq ON qq.question_id = q.id
     WHERE qq.nid = :nid
     ORDER BY qq.weight, q.id',
    array(':nid' => $nid)
  )->fetchAll();
}

/**
 * Returns the correct answer text for a question revision, for display when
 * the user answered incorrectly.
 *
 * For question types with multiple correct answers (mc_multi, mc_any), all
 * correct answers are returned as a comma-separated string.
 */
function _quiz_kit_correct_answer_display($type, $question_vid) {
  $texts = db_query(
    'SELECT answer_text FROM {quiz_kit_answer} WHERE question_vid = :vid AND is_correct = 1 ORDER BY weight',
    array(':vid' => $question_vid)
  )->fetchCol();
  return implode(', ', $texts);
}

/**
 * Loads answer options for a given question revision.
 */
function _quiz_kit_load_answers($question_vid) {
  return db_query(
    'SELECT * FROM {quiz_kit_answer} WHERE question_vid = :vid ORDER BY weight, id',
    array(':vid' => $question_vid)
  )->fetchAll();
}

/**
 * Computes quiz pages grouped by section (Option B).
 *
 * Returns an array of pages. Each page is an array of groups:
 *   array('section' => $section_object_or_NULL, 'questions' => [...])
 *
 * Sections are iterated in weight order. Questions belonging to each section
 * are kept together regardless of their individual weight values, matching the
 * grouped display in the Questions admin tab. A new page is started before a
 * section when that section has page_break = 1 and the current page already
 * has content.
 *
 * @param int   $nid           The quiz node ID.
 * @param array $all_questions All questions for the quiz (from _quiz_kit_load_questions).
 *
 * @return array
 */
function _quiz_kit_compute_pages($nid, $all_questions) {
  // Load sections in weight order.
  $sections = db_query(
    'SELECT * FROM {quiz_kit_section} WHERE nid = :nid ORDER BY weight, id',
    array(':nid' => $nid)
  )->fetchAllAssoc('id');

  if (empty($sections)) {
    // No sections — single page, one group containing all questions.
    return array(
      array(array('section' => NULL, 'questions' => $all_questions)),
    );
  }

  // Index questions by section_id, preserving question weight order.
  $by_section  = array();
  $unsectioned = array();
  foreach ($all_questions as $q) {
    $sid = (int) $q->section_id;
    if ($sid > 0 && isset($sections[$sid])) {
      $by_section[$sid][] = $q;
    }
    else {
      $unsectioned[] = $q;
    }
  }

  $pages               = array();
  $current_page_groups = array();

  // Unsectioned questions appear first on the initial page (no page break).
  if (!empty($unsectioned)) {
    $current_page_groups[] = array('section' => NULL, 'questions' => $unsectioned);
  }

  // Iterate sections in weight order.
  foreach ($sections as $sid => $section) {
    if (empty($by_section[$sid])) {
      continue; // Skip sections that have no questions.
    }

    // Start a new page before this section if configured and the current
    // page already has content.
    if ($section->page_break && !empty($current_page_groups)) {
      $pages[]             = $current_page_groups;
      $current_page_groups = array();
    }

    $current_page_groups[] = array(
      'section'   => $section,
      'questions' => $by_section[$sid],
    );
  }

  // Flush the final page.
  if (!empty($current_page_groups)) {
    $pages[] = $current_page_groups;
  }

  if (empty($pages)) {
    $pages = array(array(array('section' => NULL, 'questions' => array())));
  }

  return $pages;
}

/**
 * Saves per-section scores for a completed quiz attempt.
 *
 * Only called when field_quiz_show_section_scores is enabled on the quiz.
 *
 * @param int   $result_id  The quiz_kit_result.id.
 * @param array $all_qs     All question objects for the quiz.
 * @param array $grade_map  Map of question_id => TRUE/FALSE correctness.
 */
function _quiz_kit_save_section_scores($result_id, $all_qs, $grade_map) {
  // Group question correctness by section_id (skipping unsectioned questions).
  $by_section = array();
  foreach ($all_qs as $q) {
    $sid = (int) $q->section_id;
    if ($sid > 0 && (int) $q->scored) {
      $by_section[$sid][] = $q;
    }
  }

  foreach ($by_section as $section_id => $qs) {
    $total   = count($qs);
    $correct = 0;
    foreach ($qs as $q) {
      if (!empty($grade_map[$q->id])) {
        $correct++;
      }
    }
    $section_score = $total > 0 ? round(($correct / $total) * 100, 1) : 0;

    db_insert('quiz_kit_result_section')
      ->fields(array(
        'result_id'  => $result_id,
        'section_id' => $section_id,
        'score'      => $section_score,
      ))
      ->execute();
  }
}

/**
 * Builds a form element for a single question.
 *
 * @param object $question  The question object.
 * @param array  $answers   Answer option objects for this question.
 * @param bool   $required  Whether the question must be answered before submit.
 */
function _quiz_kit_question_element($question, $answers, $required = TRUE) {
  $text   = db_query('SELECT question_text FROM {quiz_kit_question_revision} WHERE vid = :vid', array(':vid' => $question->vid))->fetchField();
  $layout = (isset($question->answer_layout) && $question->answer_layout === 'vertical') ? 'vertical' : 'horizontal';

  $element = array(
    '#type'       => 'fieldset',
    '#title'      => check_plain($text),
    '#attributes' => array('class' => array('quiz-kit-question', 'quiz-kit-layout-' . $layout)),
  );

  switch ($question->type) {
    case 'mc_single':
    case 'mc_any':
    case 'truefalse':
      $options = array();
      foreach ($answers as $a) {
        $options[$a->id] = check_plain($a->answer_text);
      }
      $element['answer'] = array(
        '#type'     => 'radios',
        '#title'    => t('Your answer'),
        '#options'  => $options,
        '#required' => $required,
      );
      break;

    case 'mc_multi':
      $options = array();
      foreach ($answers as $a) {
        $options[$a->id] = check_plain($a->answer_text);
      }
      $element['answer'] = array(
        '#type'     => 'checkboxes',
        '#title'    => t('Select all that apply'),
        '#options'  => $options,
        '#required' => $required,
      );
      break;

    case 'short_text':
      $element['answer'] = array(
        '#type'     => 'textfield',
        '#title'    => t('Your answer'),
        '#required' => $required,
      );
      break;

    case 'long_text':
      $element['answer'] = array(
        '#type'     => 'textarea',
        '#title'    => t('Your response'),
        '#required' => $required,
        '#rows'     => 4,
      );
      break;
  }

  return $element;
}

/**
 * Grades a single answer. Returns TRUE if correct.
 */
function _quiz_kit_grade_answer($question, $given) {
  switch ($question->type) {
    case 'mc_single':
    case 'truefalse':
      $correct_id = db_query(
        'SELECT id FROM {quiz_kit_answer} WHERE question_vid = :vid AND is_correct = 1',
        array(':vid' => $question->vid)
      )->fetchField();
      return (int) $given === (int) $correct_id;

    case 'mc_any':
      $correct_ids = db_query(
        'SELECT id FROM {quiz_kit_answer} WHERE question_vid = :vid AND is_correct = 1',
        array(':vid' => $question->vid)
      )->fetchCol();
      return in_array((int) $given, array_map('intval', $correct_ids));

    case 'mc_multi':
      $correct_ids = db_query(
        'SELECT id FROM {quiz_kit_answer} WHERE question_vid = :vid AND is_correct = 1',
        array(':vid' => $question->vid)
      )->fetchCol();
      $given_ids = array_keys(array_filter((array) $given));
      sort($correct_ids);
      sort($given_ids);
      return $correct_ids == $given_ids;

    case 'short_text':
      $correct = db_query(
        'SELECT answer_text FROM {quiz_kit_answer} WHERE question_vid = :vid AND is_correct = 1',
        array(':vid' => $question->vid)
      )->fetchField();
      return mb_strtolower(trim((string) $given)) === mb_strtolower(trim((string) $correct));

    case 'long_text':
      return FALSE; // Unscored; grading loop checks $q->scored before counting.
  }

  return FALSE;
}

/**
 * Returns a per-node quiz setting, falling back to a default.
 *
 * @todo Replace stub with field_get_items() reads once fields are defined.
 */
function _quiz_kit_node_setting($node, $setting, $default) {
  $field_name = 'field_quiz_' . $setting;
  $items = field_get_items('node', $node, $field_name);
  if ($items && isset($items[0]['value'])) {
    return $items[0]['value'];
  }
  // Fall back to global config, then hardcoded default.
  $config_key = 'default_' . $setting;
  $global = config_get('quiz_kit.settings', $config_key);
  return $global !== NULL ? $global : $default;
}
