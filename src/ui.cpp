#include "ui.h"

#include <memory>
#include <vector>

#include <glibmm/main.h>
#include <gtkmm/main.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/textview.h>
#include <gtkmm/widget.h>

#include "debug.h"
#include "gettext.h"
#include "utils.h"

using namespace std;
using namespace Gtk;
using namespace Glib;

// TODO: eliminate all the global objects, as their initialization is not well defined
static TextView *lyricView;
static ScrolledWindow *lyricbar;
static RefPtr<TextBuffer> refBuffer;
static RefPtr<TextTag> tagItalic, tagBold, tagLarge, tagCenter;
static vector<RefPtr<TextTag>> tagsTitle, tagsArtist;

void set_lyrics(DB_playItem_t *track, ustring lyrics) {
	signal_idle().connect_once([track, lyrics = move(lyrics)] {
		ustring artist, title;
		{
			pl_lock_guard guard;

			if (!is_playing(track))
				return;
			artist = deadbeef->pl_find_meta(track, "artist") ?: _("Unknown Artist");
			title  = deadbeef->pl_find_meta(track, "title") ?: _("Unknown Title");
		}
		refBuffer->erase(refBuffer->begin(), refBuffer->end());
		refBuffer->insert_with_tags(refBuffer->begin(), title, tagsTitle);
		refBuffer->insert_with_tags(refBuffer->end(), ustring{"\n"} + artist + "\n\n", tagsArtist);

		bool italic = false;
		bool bold = false;
		size_t prev_mark = 0;
		vector<RefPtr<TextTag>> tags;
		while (prev_mark != ustring::npos) {
			size_t italic_mark = lyrics.find("''", prev_mark);
			if (italic_mark == ustring::npos) {
				refBuffer->insert(refBuffer->end(), lyrics.substr(prev_mark));
				break;
			}
			size_t bold_mark = ustring::npos;
			if (italic_mark < lyrics.size() - 2 && lyrics[italic_mark + 2] == '\'')
				bold_mark = italic_mark;

			tags.clear();
			if (italic) tags.push_back(tagItalic);
			if (bold)   tags.push_back(tagBold);
			refBuffer->insert_with_tags(refBuffer->end(),
			                            lyrics.substr(prev_mark, min(bold_mark, italic_mark) - prev_mark),
			                            tags);

			if (bold_mark == ustring::npos) {
				prev_mark = italic_mark + 2;
				italic = !italic;
			} else {
				prev_mark = bold_mark + 3;
				bold = !bold;
			}
		}
		last = track;
	});
}

Justification get_justification() {
	int align = deadbeef->conf_get_int("lyricbar.lyrics.alignment", 1);
	switch (align) {
		case 0:
			return JUSTIFY_LEFT;
		case 2:
			return JUSTIFY_RIGHT;
		default:
			return JUSTIFY_CENTER;
	}
}

extern "C"
GtkWidget *construct_lyricbar() {
  Gtk::Main::init_gtkmm_internals();
  refBuffer = TextBuffer::create();

  tagItalic = refBuffer->create_tag();
  tagItalic->property_style() = Pango::STYLE_ITALIC;

  tagBold = refBuffer->create_tag();
  tagBold->property_weight() = Pango::WEIGHT_BOLD;

  tagLarge = refBuffer->create_tag();
  tagLarge->property_scale() = Pango::SCALE_LARGE;

  tagCenter = refBuffer->create_tag();
  tagCenter->property_justification() = JUSTIFY_CENTER;

  // Create a new tag for font size
  auto tagFontSize = refBuffer->create_tag();
  tagFontSize->property_size() = 14 * PANGO_SCALE; // Set font size to 12 pt

  // Add the new font size tag to existing title and artist tags
  tagsTitle = {tagLarge, tagBold, tagCenter, tagFontSize};
  tagsArtist = {tagItalic, tagCenter, tagFontSize};

  lyricView = new TextView(refBuffer);
  lyricView->set_editable(false);
  lyricView->set_can_focus(false);
  lyricView->set_justification(get_justification());
  lyricView->set_wrap_mode(WRAP_WORD_CHAR);
  if (get_justification() == JUSTIFY_LEFT) {
    lyricView->set_left_margin(20);
  }
  lyricView->show();

  lyricbar = new ScrolledWindow();
  lyricbar->add(*lyricView);
  lyricbar->set_policy(POLICY_AUTOMATIC, POLICY_AUTOMATIC);

  return GTK_WIDGET(lyricbar->gobj());
}

extern "C"
int message_handler(struct ddb_gtkui_widget_s*, uint32_t id, uintptr_t ctx, uint32_t, uint32_t) {
	auto event = reinterpret_cast<ddb_event_track_t *>(ctx);
	switch (id) {
		case DB_EV_CONFIGCHANGED:
			debug_out << "CONFIG CHANGED\n";
			signal_idle().connect_once([]{ lyricView->set_justification(get_justification()); });
			break;
		case DB_EV_SONGSTARTED:
			debug_out << "SONG STARTED\n";
		case DB_EV_TRACKINFOCHANGED:
			if (!event->track || event->track == last || deadbeef->pl_get_item_duration(event->track) <= 0)
				return 0;
			auto tid = deadbeef->thread_start(update_lyrics, event->track);
			deadbeef->thread_detach(tid);
			break;
	}

	return 0;
}

extern "C"
void lyricbar_destroy() {
	delete lyricbar;
	delete lyricView;
	tagsArtist.clear();
	tagsTitle.clear();
	tagLarge.reset();
	tagBold.reset();
	tagItalic.reset();
	refBuffer.reset();
}

