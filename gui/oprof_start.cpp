/* COPYRIGHT (C) 2001 Philippe Elie
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>

#include <sstream>
#include <iostream>
#include <fstream>

#include <qcombobox.h> 
#include <qlistbox.h>
#include <qfiledialog.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qobjectlist.h>
#include <qtabwidget.h>
#include <qmessagebox.h>
#include <qvalidator.h>
#include <qlabel.h>

#include "oprof_start.h"

// TODO: some ~0u here for CRT_ALL

namespace {

inline double ratio(double x1, double x2)
{
	return fabs(((x1 - x2) / x2)) * 100;
}

bool check_and_create_config_dir()
{
	// create the directory if necessary.
	std::string dir = get_user_filename(".oprofile");

	if (access(dir.c_str(), F_OK)) {
		if (mkdir(dir.c_str(), R_OK|W_OK|X_OK)) {
			std::ostringstream out;
			out << "unable to create " << dir << " directory: ";
			QMessageBox::warning(0, 0, out.str().c_str());

			return false;
		}
	}

	return true;
}

int do_exec_command(const std::string& cmd)
{
	std::ostringstream out;
	std::ostringstream err;

	int ret = exec_command(cmd, out, err);

	if (ret) {
		// TODO: output to a window error message or a tree view error
#if 0		// gives very bad output: qt message box do not cut long line.
		std::string error = "command fail:\n" + cmd + "\n";
		error += out.str() + "\n" + err.str();;

		QMessageBox::warning(0, 0, error.c_str());
#else
		std::string error = "command fail:\n" + cmd + "\n";
		error += out.str() + "\n" + err.str();;

		std::cout << error;
#endif
	}

	return ret;
}

QString do_open_file_or_dir(QString base_dir, bool dir_only)
{
	QString result;

	if (dir_only) {
		result = QFileDialog::getExistingDirectory(base_dir, 0,
			   "open_file_or_dir", "Get directory name", true);
	} else {
		result = QFileDialog::getOpenFileName(base_dir, 0, 0,
			   "open_file_or_dir", "Get filename");
	}

	return result;
}

// like posix shell utils basename, do not append trailing '/' to result.
std::string basename(const std::string& path_name)
{
	std::string result = path_name;

	// remove all trailing '/'
	size_t last_delimiter = result.find_last_of('/');
	if (last_delimiter != std::string::npos) {
		while (last_delimiter && result[last_delimiter] == '/')
			--last_delimiter;

		result.erase(last_delimiter);
	}

	last_delimiter = result.find_last_of('/');
	if (last_delimiter != std::string::npos)
		result.erase(last_delimiter);

	return result;
}

} // anonymous namespace

op_event_descr::op_event_descr()
	: 
	counter_mask(0),
	val(0),
	unit(0),
	um_descr(0),
	name(0),
	help_str(0),
	min_count(0),
	cb(0)
{
}

oprof_start::oprof_start()
	:
	oprof_start_base(0, 0, false, 0),
	validate_buffer_size(new QIntValidator(buffer_size_edit)),
	validate_hash_table_size(new QIntValidator(hash_table_size_edit)),
	validate_event_count(new QIntValidator(event_count_edit)),
	validate_pid_filter(new QIntValidator(pid_filter_edit)),
	validate_pgrp_filter(new QIntValidator(pgrp_filter_edit)),
	last_mouse_motion_cb_index((uint)-1),
	cpu_type(op_get_cpu_type()),
	op_nr_counters(2)
{
	if (cpu_type == CPU_ATHLON)
		op_nr_counters = 4;

	// validator range/value are set only when we have build the
	// description of events.
	buffer_size_edit->setValidator(validate_buffer_size);
	hash_table_size_edit->setValidator(validate_hash_table_size);
	event_count_edit->setValidator(validate_event_count);
	pid_filter_edit->setValidator(validate_pid_filter);
	pgrp_filter_edit->setValidator(validate_pgrp_filter);

	int cpu_mask = 1 << cpu_type;

	load_config_file();

	// setup the configuration page.
	kernel_filename_edit->setText(config.kernel_filename.c_str());
	map_filename_edit->setText(config.map_filename.c_str());

	// this do not work perhaps we need to derivate QIntValidator to get a
	// better handling if the validation?
//	validate_buffer_size->setValue(config.buffer_size);

	buffer_size_edit->setText(QString().setNum(config.buffer_size));
	hash_table_size_edit->setText(QString().setNum(config.hash_table_size));
	pid_filter_edit->setText(QString().setNum(config.pid_filter));
	pgrp_filter_edit->setText(QString().setNum(config.pgrp_filter));
	base_opd_dir_edit->setText(config.base_opd_dir.c_str());
	ignore_daemon_samples_cb->setChecked(config.ignore_daemon_samples);
	kernel_only_cb->setChecked(config.kernel_only);

	// the unit mask check boxes
	check1->hide();
	check2->hide();
	check3->hide();
	check4->hide();
	check5->hide();
	check6->hide();
 
	// build from stuff in op_events.c the description of events.
	for (uint i = 0 ; i < op_nr_events ; ++i) {
		if (op_events[i].cpu_mask & cpu_mask) {
			op_event_descr descr;

			descr.counter_mask = op_events[i].counter_mask;
			descr.val = op_events[i].val;
			if (op_events[i].unit) {
				descr.unit = &op_unit_masks[op_events[i].unit];
				descr.um_descr = &op_unit_descs[op_events[i].unit];
			} else {
				descr.unit = 0;
				descr.um_descr = 0;
			}

			descr.name = op_events[i].name;
			descr.help_str = op_event_descs[i];
			descr.min_count = op_events[i].min_count;

			// These event_cfg set acts like as default value if
			// the event config does not exist.

			// min value * 100 is the default value.
			event_cfg[descr.name].count = descr.min_count * 100;
			event_cfg[descr.name].umask = 0;
			if (descr.unit)
				event_cfg[descr.name].umask = descr.unit->default_mask;
			event_cfg[descr.name].os_ring_count = 1;
			event_cfg[descr.name].user_ring_count = 1;

			v_events.push_back(descr);
		}
	}

	validate_buffer_size->setRange(OP_MIN_BUFFER_SIZE, OP_MAX_BUFFER_SIZE);
	validate_hash_table_size->setRange(OP_MIN_HASH_TABLE_SIZE, 
					   OP_MAX_HASH_TABLE_SIZE);

	validate_pid_filter->setRange(OP_MIN_PID, OP_MAX_PID);
	validate_pgrp_filter->setRange(OP_MIN_PID, OP_MAX_PID);

	for (uint i = 0; i < op_nr_counters; ++i) {
		std::string str("Counter ");
		char c = '1' + i;
		counter_combo->insertItem((str + c).c_str());
	}
	load_event_config_file();
}

// load the configuration, if the configuration file does not exist create it.
// the parent directory of the config file is created if necessary through
// save_config_file().
void oprof_start::load_config_file()
{
	std::string name = get_user_filename(".oprofile/oprof_start_config");

	{
		std::ifstream in(name.c_str());
		if (!in)
			// trick: this create the config directory if necessary
			save_config_file();
	}

	std::ifstream in(name.c_str());
	if (!in) {
		QMessageBox::warning(this, 0, "Unable to open configuration "
				     "~/.oprofile/oprof_start_config");
		return;
	}

	in >> config;
}

// save the configuration by overwritting the configuration file if it exist or
// create it. The parent directory of the config file is created if necessary
bool oprof_start::save_config_file()
{
	if (check_and_create_config_dir() == false)
		return false;

	std::string name = get_user_filename(".oprofile/oprof_start_config");

	std::ofstream out(name.c_str());
	
	out << config;

	return true;
}

// this work as load_config_file()/save_config_file()
void oprof_start::load_event_config_file()
{
	std::string name = get_user_filename(".oprofile/oprof_start_event");

	{
		std::ifstream in(name.c_str());
		if (!in)
			// trick: this create the config directory if necessary
			save_event_config_file();
	}

	std::ifstream in(name.c_str());
	if (!in) {
		QMessageBox::warning(this, 0, "Unable to open configuration "
				     "~/.oprofile/oprof_start_event");
		return;
	}

	// TODO: need checking on the key validity :(
	in >> event_cfg;

	event_cfg.set_dirty(false);
}

// this work as load_config_file()/save_config_file()
bool oprof_start::save_event_config_file()
{
	if (check_and_create_config_dir() == false)
		return false;

	std::string name = get_user_filename(".oprofile/oprof_start_event");

	std::ofstream out(name.c_str());

	out << event_cfg;

	event_cfg.set_dirty(false);

	return true;
}

// user is happy and want to quit in the normal way, so save the config file.
void oprof_start::accept()
{
	// Configuration for an event is recorded only when the user change
	// the currently selected events, so force the recording here.
	record_selected_event_config();

	// TODO: check and warn about return code.
	if (event_cfg.dirty()) {
		printf("saving configuration file");
		save_event_config_file();
	}

	record_config();

	save_config_file();

	QDialog::accept();
}

// counter combo box selected
void oprof_start::counter_selected(int ctr)
{
	events_list->clear();
 
	for (std::vector<op_event_descr>::const_iterator cit = v_events.begin();
		cit != v_events.end(); ++cit) {
 
		// FIXME: counter_mask check

		new QListViewItem(events_list, cit->name, cit->help_str); 
	}
}

 
// event selected
void oprof_start::event_selected(QListViewItem * item)
{
	op_event_descr const * descr = locate_event(item->text(0).latin1());

	std::string str(descr->help_str);
	str += " (min count: FIXME)";

	event_help_label->setText(str.c_str());
 
	// FIXME: here we make visible the right checkboxes and set their
	// tooltips and text.
}
 
 
// user need to select a file or directory, distinction about what the user
// needs to select is made through the source of the qt event.
void oprof_start::on_choose_file_or_dir()
{
	const QObject* source = sender();

	/* FIXME: yuck. let's just have separate slots for each event */
	if (source) {
		bool dir_only = false;
		QString base_dir;

		if (source->name() == QString("base_opd_dir_tb") ||
		    source->name() == QString("samples_files_dir_tb")) {
			dir_only = true;
			base_dir = base_opd_dir_edit->text();
		} else if (source->name() == QString("kernel_filename_tb")) {
			// need a stl to qt adapter?
			std::string result;
			std::string name=kernel_filename_edit->text().latin1();
			result = basename(name);
			base_dir = name.c_str();
		} else if (source->name() == QString("map_filename_tb")) {
			// need a stl to qt adapter?
			std::string result;
			std::string name = map_filename_edit->text().latin1();
			result = basename(name);
			base_dir = name.c_str();
		} else {
			base_dir = base_opd_dir_edit->text();
		}
		
		// the association between a file open tool button and the
		// edit widget is made through its name base on the naming
		// convention: object_name_tb --> object_name_edit
		QString result = do_open_file_or_dir(base_dir, dir_only);
		if (result.length()) {
			std::string src_name(source->name());
			// we support only '_tb' suffix, the right way is to
			// remove the '_' suffix and append the '_edit' suffix
			src_name = src_name.substr(0, src_name.length() - 3);
			src_name += "_edit";

			QObject* edit = child(src_name.c_str(), "QLineEdit");
			if (edit) {
				reinterpret_cast<QLineEdit*>(edit)->setText(result);
			}
		}
	} else {
		// Impossible if the dialog is well designed, if you see this
		// message you must make something sensible if source == 0
		fprintf(stderr, "oprof_start::on_choose_file_or_dir() slot is"
			"not directly call-able\n");
	}
}

// this record the curernt selected event setting in the event_cfg[] stuff.
// event_cfg->dirty is set only if change is recorded.
// TODO: need validation?
void oprof_start::record_selected_event_config()
{
#if 0 
	// saving must be made only if a change occur to avoid setting the
	// dirty flag in event_cfg. For the same reason we can not use a
	// temporay to an 'event_setting&' to clarify the code.
	// This come from:
	// if (event_cfg[name].xxx == yyyy) 
	// call the non const operator [] ofevent_cfg. We can probably do
	// better in event_cfg stuff but it need a proxy return from []. 
	// See persistent_config_t doc
	if (event_selected) {
		const persistent_config_t<event_setting>& cfg = event_cfg;

		uint count = event_count_edit->text().toUInt();

		if (cfg[event_selected->name].count != count)
			event_cfg[event_selected->name].count = count;

		if (cfg[event_selected->name].os_ring_count !=
		    os_ring_count_cb->isChecked())
			event_cfg[event_selected->name].os_ring_count = 
				os_ring_count_cb->isChecked();
		
		if (cfg[event_selected->name].user_ring_count !=
		    user_ring_count_cb->isChecked())
			event_cfg[event_selected->name].user_ring_count =
				user_ring_count_cb->isChecked();

		if (cfg[event_selected->name].umask != get_unit_mask())
			event_cfg[event_selected->name].umask = 
				get_unit_mask();
	}
#endif
}

// same design as record_selected_event_config without trying to not dirties
// the config.dirty() flag, so config is always saved when user quit. This
// function also validate the result (The actual qt validator installed
// are not sufficient to do the validation)
bool oprof_start::record_config()
{
	config.kernel_filename = kernel_filename_edit->text().latin1();
	config.map_filename = map_filename_edit->text().latin1();
	
	if (config.buffer_size < OP_MIN_BUFFER_SIZE || 
	    config.buffer_size > OP_MAX_BUFFER_SIZE) {
		std::ostringstream error;

		error << "buffer size out of range: " << config.buffer_size
		      << " valid range is [" << OP_MIN_BUFFER_SIZE << ", "
		      << OP_MAX_BUFFER_SIZE << "]";

		QMessageBox::warning(this, 0, error.str().c_str());

		return false;
	}

	config.buffer_size = buffer_size_edit->text().toUInt();

	if (config.hash_table_size < OP_MIN_HASH_TABLE_SIZE || 
	    config.hash_table_size > OP_MAX_HASH_TABLE_SIZE) {
		std::ostringstream error;

		error << "hash table size out of range: " 
		      << config.hash_table_size
		      << " valid range is [" << OP_MIN_HASH_TABLE_SIZE << ", "
		      << OP_MAX_HASH_TABLE_SIZE << "]";

		QMessageBox::warning(this, 0, error.str().c_str());

		return false;
	}

	config.hash_table_size = hash_table_size_edit->text().toUInt();
	config.pid_filter = pid_filter_edit->text().toUInt();
	config.pgrp_filter = pgrp_filter_edit->text().toUInt();
	config.base_opd_dir = base_opd_dir_edit->text().latin1();
	config.ignore_daemon_samples = ignore_daemon_samples_cb->isChecked();
	config.kernel_only = kernel_only_cb->isChecked();

	if (config.base_opd_dir.length() && 
	    config.base_opd_dir[config.base_opd_dir.length()-1] != '/')
		config.base_opd_dir += '/';

	return true;
}

// helper to get_unit_mask. assert cb != 0
uint oprof_start::get_unit_mask_part(const QCheckBox* cb, uint mask)
{
#if 0 
	// trick: the name of cb is an integer which can index the unit_mask
	// stuff
	QString name = cb->name();
	uint idx = name.toUInt();
	const op_unit_mask* um = event_selected->unit;
	if  (idx >= 0 && idx < um->num) {
		if (cb->isChecked()) {
			if (um->unit_type_mask == utm_bitmask)
				mask |= um->um[idx];
			else {
				if (mask) {
					QMessageBox::warning(this, 0, "oprof_start::get_unit_mask() exclusive mask get multiple definition");
				}
				mask = um->um[idx];
			}
		}
	}

	return mask;
#endif 
}

// return the unit mask selected through the unit mask check box
uint oprof_start::get_unit_mask()
{
#if 0 
	uint mask = 0;

	if (event_selected && event_selected->unit) {
		QObjectList* l = 
			const_cast<QObjectList*>(unit_mask_group->children());
		if (l) {
			for (QObject* cur = l->first(); cur; cur = l->next()) {
				QCheckBox* cb = dynamic_cast<QCheckBox*>(cur);
				if (cb) {
					mask = get_unit_mask_part(cb, mask);
				}
			}
		
			// FIXME docs says: do it but this segfault
			// delete l;
		}
	}

	return mask;
#endif 
}

// create and setup the unit mask btn stuff. assert descr->unit != 0 &&
// descr->um_descr != 0
void oprof_start::create_unit_mask_btn(const op_event_descr* descr)
{
#if 0 
	const op_unit_mask* um = descr->unit;
	const op_unit_desc* um_desc = descr->um_descr;

	if (!um || !um_desc) {
		QMessageBox::critical(this, 0, "oprof_start::create_unit_mask_btn descr->unit == 0 or descr->um_desc == 0");

		exit(EXIT_FAILURE);
	}

	// assert um->unit_type_mask != utm_mandatory

	// we need const access. see record_selected_event_config()
	const persistent_config_t<event_setting>& cfg = event_cfg;

	if (um->unit_type_mask == utm_exclusive)
		unit_mask_group->setExclusive(true);

	for (size_t i = 0; i < um->num ; ++i) {
		// allow to use the name of unit mask checkbox as an index of
		// which button has been selected. Would be unique? If you
		// change the name scheming you must modify get_unit_mask
		QString name;
		name.setNum(i);

		QCheckBox* btn =  new QCheckBox(um_desc->desc[i],
						unit_mask_group, 
						name.latin1());

		btn->setFixedSize(btn->sizeHint());

		// FIXME: 20 must be get at run time?
		btn->move(20, (i * btn->height()) + 20);

		btn->show();

		if (um->unit_type_mask == utm_exclusive) {
			if (cfg[descr->name].umask == um->um[i])
				btn->setChecked(true);
		} else {
			if (i == um->num - 1) {
				if (cfg[descr->name].umask == um->um[i])
					btn->setChecked(true);
			} else {
				if (cfg[descr->name].umask & um->um[i])
					btn->setChecked(true);
			}
		}
	}
#endif 
}

// the event selected has been perhaps changed, descr is the new event selected
// event_selected the old.
void oprof_start::do_selected_event_change(const op_event_descr* descr)
{
#if 0 
	if (!descr || descr == event_selected)
		return;

	// record the new setting in the event_cfg stuff.
	record_selected_event_config();

	validate_event_count->setRange(descr->min_count, OP_MAX_PERF_COUNT);

	if (event_selected) {
		event_selected->cb->setBackgroundMode(event_background_mode);
	}

	descr->cb->setBackgroundMode(PaletteMidlight);

	// unit_mask_frame is the container of unit mask object the once
	// child is the QButtonGroup unit_mask_group we delete unit_mask_goup
	// to avoid buggy redisplay and to get less screen flickering. The only
	// purpose of unit_mask_frame is to provide the size and position of
	// the unit_mask_group.

	// do not assume than unit_mask_group title is hard-coded so changing
	// it in the designer or at oprof_start() ctr time is sufficient.
	QString um_group_title = unit_mask_group->title();

	delete unit_mask_group;
	unit_mask_group = new QButtonGroup(um_group_title, unit_mask_frame);

	unit_mask_group->setFixedSize(unit_mask_frame->size());
	unit_mask_group->show();

	const op_unit_mask* um = descr->unit;

	if (um && um->unit_type_mask != utm_mandatory) {
		create_unit_mask_btn(descr);
	}

	// we need const access. see record_selected_event_config()
	const persistent_config_t<event_setting>& cfg = event_cfg;

	os_ring_count_cb->setChecked(cfg[descr->name].os_ring_count);
	user_ring_count_cb->setChecked(cfg[descr->name].user_ring_count);

	QString count_text;
	count_text.setNum(cfg[descr->name].count);
	event_count_edit->setText(count_text);

	event_selected = descr;
#endif 
}

// helper for on_event_clicked(). This function is complicated by assymetric
// counter events eg P6 which allow a few evebt only in counter 0 and a few
// other only in counter 1. For now P6 is the only arch with this "features"
// so we handle it in an ugly but simple way.
void oprof_start::event_checked(const op_event_descr* descr)
{
#if 0
	// sanity checking.
	size_t i;
	for (i = 0 ; i < v_events_selected.size() ; ++i) {
		if (descr == v_events_selected[i]) {
			break;
		}
	}

	if (v_events_selected.size() && i != v_events_selected.size()) {
		printf("oprof_start::on_event_clicked(): try "
		       "to insert already selected event\n");
		return;		// Fatal ?
	}

	// some arch needs to use specific counters for some events. For now
	// only the P6 core have this, so I use a specific trick for P6 which
	// can not work in the general case. The general solution is to use an
	// algo that treat counter as resource to allocate for events with an
	// optimal allocation order (something like an inteference graph would
	// work)
	int bit_mask = 0;
	for (size_t j = 0; j < v_events_selected.size(); ++j)
		if (v_events_selected[j]->counter_mask != ~0u)
			bit_mask |= v_events_selected[j]->counter_mask;

	if (descr->counter_mask != ~0u && descr->counter_mask & bit_mask) {
		descr->cb->setChecked(false);
				
		// FIXME: document
		QMessageBox::warning(this, 0, "The selected event use a counter already used by another event, check the html doc");
		return;
	}

	bool swap_counter = false;
	// ugly: work only for the P6 asymetric counter
	if (descr->counter_mask == 1 && bit_mask == 2) {
		swap_counter = true;
	}

	v_events_selected.push_back(descr);

	if (swap_counter)
		// ugly: work only for the P6 asymetric counter
		std::swap(v_events_selected[0], v_events_selected[1]);
#endif 
}

// helper for on_event_clicked()
void oprof_start::event_unchecked(const op_event_descr* descr)
{
#if 0 
	// sanity checking.
	size_t i;
	for (i = 0; i < v_events_selected.size(); ++i) {
		if (descr == v_events_selected[i]) {
			break;
		}
	}

	if (v_events_selected.size() == 0 || i == v_events_selected.size()) {
		printf("oprof_start::on_event_clicked(): try "
		       "to remove already unselected event\n");
		return;		// Fatal ?
	} 

	v_events_selected.erase(v_events_selected.begin() + i);
#endif 
}

// Handle event check box click. The current state of the check box is set by
// Qt before calling this slot.
void oprof_start::on_event_clicked()
{
#if 0 
	QObject* source = const_cast<QObject*>(sender());

	// we don't disable the counter setup page to allow user to change
	// setting even when the checkbox is un-selected. User get a visual
	// feedback of which event check box is currently selected.
	if (source) {

		QCheckBox* cb = dynamic_cast<QCheckBox*>(source);
		if (cb == 0) {
			// fatal ?
			fprintf(stderr, "oprof_start::on_event_clicked() event"
				"come from: (%s, %s), expect QCheckBox\n",
				source->className(), source->name());

			return;
		}

		const op_event_descr* descr = locate_event(source->name());
		if (descr->cb != cb) {
			QMessageBox::critical(this, 0, "oprof_start::on_event_clicked() cb != descr->cb");

			exit(EXIT_FAILURE);
		}

		if (cb->isChecked()) {
			if (v_events_selected.size() >= op_nr_counters) {
				cb->setChecked(false);

				QMessageBox::warning(this, 0, "Too many selected events for this architecture.");

				return;
			}
		}

		do_selected_event_change(descr);

		if (cb->isChecked()) {
			event_checked(descr);
		} else {
			event_unchecked(descr);
		}
	}
#endif 
}

// called on explict user request through btn "flush profiler data" and also
// when the profiler is stopped. (start the profiler also try to stop the
// profiling if on so start profiler imply also a flush of data)
void oprof_start::on_flush_profiler_data()
{
	if (is_profiler_started()) {
		do_exec_command("op_dump");
	} else {
		QMessageBox::warning(this, 0, "The profiler is not started.");
	}
}

// user is happy of its setting.
void oprof_start::on_start_profiler()
{
#if 0 
	if (v_events_selected.empty()) {
		QMessageBox::warning(this, 0, 
				     "You must select at least one event");
		return;
	}

	// Configuration for an event is recorded only when the user change
	// the currently selected events, so force the recording here.
	record_selected_event_config();

	// we need const access. see record_selected_event_config()
	const persistent_config_t<event_setting>& cfg = event_cfg;

	// sanitize the counter setup.
	for (size_t i = 0; i < v_events_selected.size(); ++i) {

		// if sanitize lose we set the currently selected event on
		// the failing event and ensure than the check box is visible.

		const op_event_descr* descr = v_events_selected[i];

		if (!cfg[descr->name].os_ring_count &&
		    !cfg[descr->name].user_ring_count) {
			QMessageBox::warning(this, 0, "You must select at "
					 "least one ring count: user or os");

			do_selected_event_change(descr);
			user_ring_count_cb->setFocus();

			return;
		}

		if (cfg[descr->name].count < descr->min_count ||
		    cfg[descr->name].count > OP_MAX_PERF_COUNT) {
			std::ostringstream out;

			out << "event " << descr->name << " count of range: "
			    << cfg[descr->name].count << " must be in [ "
			    << descr->min_count << ", "
			    << OP_MAX_PERF_COUNT
			    << "]";

			QMessageBox::warning(this, 0, out.str().c_str());

			do_selected_event_change(descr);
			event_count_edit->setFocus();

			return;
		}

		if (descr->unit && 
		    descr->unit->unit_type_mask != utm_exclusive &&
		    cfg[descr->name].umask == 0) {
			std::ostringstream out;

			out << "event " << descr->name<< " invalid unit mask: "
			    << cfg[descr->name].umask << std::endl;

			QMessageBox::warning(this, 0, out.str().c_str());

			do_selected_event_change(descr);

			// difficult to put the focus, don't take care?

			return;
		}
	}

	if (is_profiler_started()) {
		int user_choice = 
			QMessageBox::warning(this, 0, 
					     "Profiler already started:\n\n"
					     "stop and restart it?", 
					     "Restart", "Cancel", 0, 0, 1);

		if (user_choice == 1)
			return;

		// this flush profiler data also.
		on_stop_profiler();
	}

	// build the cmd line.
	std::ostringstream cmd_line;

	cmd_line << "op_start";

	for (size_t i = 0; i < v_events_selected.size(); ++i) {
		const op_event_descr* descr = v_events_selected[i];

		cmd_line << " --ctr" << i << "-event=" << descr->name;
		cmd_line << " --ctr" << i << "-count=" 
			 << cfg[descr->name].count;
		cmd_line << " --ctr" << i << "-kernel=" 
			 << cfg[descr->name].os_ring_count;
		cmd_line << " --ctr" << i << "-user=" 
			 << cfg[descr->name].user_ring_count;

		if (descr->um_descr) {
			cmd_line << " --ctr" << i << "-unit-mask="
				 << cfg[descr->name].umask;
		}
	}

	// record_config validate the config
	if (record_config() == false)
		return;

	cmd_line << " --map-file=" << config.map_filename;
	cmd_line << " --vmlinux=" << config.kernel_filename;
	cmd_line << " --kernel-only=" << config.kernel_only;
	cmd_line << " --pid-filter=" << config.pid_filter;
	cmd_line << " --pgrp-filter=" << config.pgrp_filter;
	cmd_line << " --base-dir=" << config.base_opd_dir;
	cmd_line << " --samples-dir=" << config.base_opd_dir 
		 << config.samples_files_dir;
	cmd_line << " --device-file=" << config.base_opd_dir 
		 << config.device_file;
	cmd_line << " --hash-map-device-file=" << config.base_opd_dir
		 << config.hash_map_device;
	cmd_line << " --log-file=" << config.base_opd_dir
		 << config.daemon_log_file;
	cmd_line << " --ignore-myself=" << config.ignore_daemon_samples;
	cmd_line << " --buffer-size=" << config.buffer_size;
	cmd_line << " --hash-table-size=" << config.hash_table_size;

//	std::cout << cmd_line.str() << std::endl;

	do_exec_command(cmd_line.str());
#endif 
}

// flush and stop the profiler if it was started.
void oprof_start::on_stop_profiler()
{
	if (is_profiler_started()) {
		if (do_exec_command("op_dump") == 0) {
			do_exec_command("op_stop");
		}
	} else {
		QMessageBox::warning(this, 0, "The profiler is already stopped.");
	}
}

// helper to retrieve an event descr through its name.
const op_event_descr* oprof_start::locate_event(const char* name)
{
	for (size_t i = 0 ; i < v_events.size() ; ++i) {
		if (std::string(v_events[i].name) == name) {
			return &v_events[i];
		}
	}

	return 0;
}