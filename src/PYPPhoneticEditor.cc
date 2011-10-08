/* vim:set et ts=4 sts=4:
 *
 * ibus-pinyin - The Chinese PinYin engine for IBus
 *
 * Copyright (c) 2011 Peng Wu <alexepico@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "PYPPhoneticEditor.h"
#include "PYConfig.h"
#include "PYPinyinProperties.h"
#include "PYSimpTradConverter.h"

using namespace PY;

/* init static members */
LibPinyinPhoneticEditor::LibPinyinPhoneticEditor (PinyinProperties &props,
                                                  Config &config):
    Editor (props, config),
    m_pinyin_len (0),
    m_lookup_table (m_config.pageSize ())
{
    m_candidates = g_array_new(FALSE, TRUE, sizeof(phrase_token_t));
}

gboolean
LibPinyinPhoneticEditor::processSpace (guint keyval, guint keycode,
                                       guint modifiers)
{
    if (!m_text)
        return FALSE;
    if (cmshm_filter (modifiers) != 0)
        return TRUE;
    if (m_lookup_table.size () != 0) {
        selectCandidate (m_lookup_table.cursorPos ());
    } else {
        commit ();
    }
    return TRUE;
}

gboolean
LibPinyinPhoneticEditor::processFunctionKey (guint keyval, guint keycode, guint modifiers)
{
    if (m_text.empty ())
        return FALSE;

    /* ignore numlock */
    modifiers = cmshm_filter (modifiers);

    if (modifiers != 0 && modifiers != IBUS_CONTROL_MASK)
        return TRUE;

    /* process some cursor control keys */
    if (modifiers == 0) {  /* no modifiers. */
        switch (keyval) {
        case IBUS_Return:
        case IBUS_KP_Enter:
            commit ();
            return TRUE;

        case IBUS_BackSpace:
            removeCharBefore ();
            return TRUE;

        case IBUS_Delete:
        case IBUS_KP_Delete:
            removeCharAfter ();
            return TRUE;

        case IBUS_Left:
        case IBUS_KP_Left:
            moveCursorLeft ();
            return TRUE;

        case IBUS_Right:
        case IBUS_KP_Right:
            moveCursorRight ();
            return TRUE;

        case IBUS_Home:
        case IBUS_KP_Home:
            moveCursorToBegin ();
            return TRUE;

        case IBUS_End:
        case IBUS_KP_End:
            moveCursorToEnd ();
            return TRUE;

        case IBUS_Up:
        case IBUS_KP_Up:
            cursorUp ();
            return TRUE;

        case IBUS_Down:
        case IBUS_KP_Down:
            cursorDown ();
            return TRUE;

        case IBUS_Page_Up:
        case IBUS_KP_Page_Up:
            pageUp ();
            return TRUE;

        case IBUS_Page_Down:
        case IBUS_KP_Page_Down:
        case IBUS_Tab:
            pageDown ();
            return TRUE;

        case IBUS_Escape:
            reset ();
            return TRUE;
        default:
            return TRUE;
        }
    } else { /* ctrl key pressed. */
        switch (keyval) {
        case IBUS_BackSpace:
            removeWordBefore ();
            return TRUE;

        case IBUS_Delete:
        case IBUS_KP_Delete:
            removeWordAfter ();
            return TRUE;

        case IBUS_Left:
        case IBUS_KP_Left:
            moveCursorLeftByWord ();
            return TRUE;

        case IBUS_Right:
        case IBUS_KP_Right:
            moveCursorToEnd ();
            return TRUE;

        default:
            return TRUE;
        }
    }
    return TRUE;
}

gboolean
LibPinyinPhoneticEditor::processKeyEvent (guint keyval, guint keycode, guint modifiers)
{
    return FALSE;
}

void
LibPinyinPhoneticEditor::updateLookupTableFast (void)
{
    Editor::updateLookupTableFast (m_lookup_table, TRUE);
}

void
LibPinyinPhoneticEditor::updateLookupTable (void)
{
    m_lookup_table.clear ();

    fillLookupTableByPage ();
    if (m_lookup_table.size()) {
        Editor::updateLookupTable (m_lookup_table, TRUE);
    } else {
        hideLookupTable ();
    }
}

gboolean
LibPinyinPhoneticEditor::fillLookupTableByPage (void)
{

    guint filled_nr = m_lookup_table.size ();
    guint page_size = m_lookup_table.pageSize ();

    /* fill lookup table by libpinyin get candidates. */
    guint need_nr = MIN (page_size, m_candidates->len - filled_nr);
    g_assert (need_nr >=0);
    if (need_nr == 0)
        return FALSE;

    for (guint i = filled_nr; i < filled_nr + need_nr; i++) {
        if (G_LIKELY (m_props.modeSimp ())) { /* Simplified Chinese */
            phrase_token_t *token = &g_array_index
                (m_candidates, phrase_token_t, i);
            char *word = NULL;
            pinyin_translate_token(m_instance, *token, &word);
            Text text (word);
            m_lookup_table.appendCandidate(text);
            g_free(word);
        } else { /* Traditional Chinese */
            m_buffer.truncate (0);
            phrase_token_t *token = &g_array_index
                (m_candidates, phrase_token_t, i);
            char *word = NULL;
            pinyin_translate_token(m_instance, *token, &word);
            SimpTradConverter::simpToTrad (word, m_buffer);
            Text text (m_buffer);
            m_lookup_table.appendCandidate (text);
            g_free(word);
        }
    }

    return TRUE;
}

void
LibPinyinPhoneticEditor::pageUp (void)
{
    if (G_LIKELY (m_lookup_table.pageUp ())) {
        updateLookupTableFast ();
        updatePreeditText ();
        updateAuxiliaryText ();
    }
}

void
LibPinyinPhoneticEditor::pageDown (void)
{
    if (G_LIKELY((m_lookup_table.pageDown ()) ||
                 (fillLookupTableByPage () && m_lookup_table.pageDown()))) {
        updateLookupTableFast ();
        updatePreeditText ();
        updateAuxiliaryText ();
    }
}

void
LibPinyinPhoneticEditor::cursorUp (void)
{
    if (G_LIKELY (m_lookup_table.cursorUp ())) {
        updateLookupTableFast ();
        updatePreeditText ();
        updateAuxiliaryText ();
    }
}

void
LibPinyinPhoneticEditor::cursorDown (void)
{
    if (G_LIKELY ((m_lookup_table.cursorPos () == m_lookup_table.size() - 1) &&
                  (fillLookupTableByPage () == FALSE))) {
        return;
    }

    if (G_LIKELY (m_lookup_table.cursorDown ())) {
        updateLookupTableFast ();
        updatePreeditText ();
        updateAuxiliaryText ();
    }
}

void
LibPinyinPhoneticEditor::candidateClicked (guint index, guint button, guint state)
{
    selectCandidateInPage (index);
}

void
LibPinyinPhoneticEditor::reset (void)
{
    m_pinyin_len = 0;
    m_lookup_table.clear ();

    Editor::reset ();
}

void
LibPinyinPhoneticEditor::update (void)
{
    guint pinyin_cursor = getPinyinCursor ();
    pinyin_get_candidates (m_instance, pinyin_cursor, m_candidates);
    updateLookupTable ();
    updatePreeditText ();
    updateAuxiliaryText ();
}

void
LibPinyinPhoneticEditor::commit (const gchar *str)
{
    StaticText text(str);
    commitText (text);
}

guint
LibPinyinPhoneticEditor::getPinyinCursor ()
{
    /* Translate cursor position to pinyin position. */
    PinyinKeyPosVector & pinyin_poses = m_instance->m_pinyin_poses;
    guint pinyin_cursor = pinyin_poses->len;
    for (size_t i = 0; i < pinyin_poses->len; ++i) {
        PinyinKeyPos *pos = &g_array_index
            (pinyin_poses, PinyinKeyPos, i);
        if (pos->get_pos () <= m_cursor && m_cursor < pos->get_end_pos ())
            pinyin_cursor = i;
    }

    g_assert (pinyin_cursor >= 0);
    return pinyin_cursor;
}

gboolean
LibPinyinPhoneticEditor::selectCandidate (guint i)
{
    guint pinyin_cursor = getPinyinCursor ();

    /* NOTE: deal with normal candidates selection here by libpinyin. */
    phrase_token_t *token = &g_array_index (m_candidates, phrase_token_t, i);
    pinyin_choose_candidate(m_instance, pinyin_cursor, *token);
    return TRUE;
}

gboolean
LibPinyinPhoneticEditor::selectCandidateInPage (guint i)
{
    guint page_size = m_lookup_table.pageSize ();
    guint cursor_pos = m_lookup_table.cursorPos ();

    if (G_UNLIKELY (i >= page_size))
        return FALSE;
    i += (cursor_pos / page_size) * page_size;

    return selectCandidate (i);
}

gboolean
LibPinyinPhoneticEditor::removeCharBefore (void)
{
    if (G_UNLIKELY (m_cursor == 0))
        return FALSE;

    m_cursor --;
    m_text.erase (m_cursor, 1);

    updatePinyin ();
    update ();

    return TRUE;
}

gboolean
LibPinyinPhoneticEditor::removeCharAfter (void)
{
    if (G_UNLIKELY (m_cursor == m_text.length ()))
        return FALSE;

    m_text.erase (m_cursor, 1);

    updatePinyin ();
    update ();

    return TRUE;
}

gboolean
LibPinyinPhoneticEditor::moveCursorLeft (void)
{
    if (G_UNLIKELY (m_cursor == 0))
        return FALSE;

    m_cursor --;
    update ();
    return TRUE;
}

gboolean
LibPinyinPhoneticEditor::moveCursorRight (void)
{
    if (G_UNLIKELY (m_cursor == m_text.length ()))
        return FALSE;

    m_cursor ++;
    update ();
    return TRUE;
}

gboolean
LibPinyinPhoneticEditor::moveCursorToBegin (void)
{
    if (G_UNLIKELY (m_cursor == 0))
        return TRUE;

    m_cursor = 0;
    update ();
    return TRUE;
}

gboolean
LibPinyinPhoneticEditor::moveCursorToEnd (void)
{
    if (G_UNLIKELY (m_cursor == m_text.length ()))
        return FALSE;

    m_cursor = m_text.length ();
    update ();
    return TRUE;
}
