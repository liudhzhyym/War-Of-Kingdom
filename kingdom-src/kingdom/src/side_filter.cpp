/* $Id: side_filter.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2010 - 2012 by Yurii Chernyi <terraninfo@terraninfo.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "global.hpp"

#include "config.hpp"
#include "foreach.hpp"
#include "log.hpp"
#include "resources.hpp"
#include "side_filter.hpp"
#include "variable.hpp"
#include "team.hpp"
#include "serialization/string_utils.hpp"

static lg::log_domain log_engine_sf("engine/side_filter");
#define ERR_NG LOG_STREAM(err, log_engine_sf)

#ifdef _MSC_VER
// This is a workaround for a VC bug; this constructor is never called
// and so we don't care about the warnings this quick fix generates
#pragma warning(push)
#pragma warning(disable:4413)
side_filter::side_filter():
	cfg_(vconfig::unconstructed_vconfig()),
	flat_(),
	side_string_()
{
	assert(false);
}
#pragma warning(pop)
#endif


side_filter::side_filter(const vconfig& cfg, bool flat_tod) :
	cfg_(cfg),
	flat_(flat_tod),
	side_string_()
{
}

side_filter::side_filter(const std::string &side_string, bool flat_tod)
	: cfg_(vconfig::empty_vconfig()), flat_(flat_tod), side_string_(side_string)
{
}

std::vector<int> side_filter::get_teams() const
{
	//@todo: replace with better implementation
	std::vector<int> result;
	foreach (const team &t, *resources::teams) {
		if (match(t)) {
			result.push_back(t.side());
		}
	}
	return result;
}

static bool check_side_number(const team &t, const std::string &str)
{
		std::vector<std::string> list = utils::split(str);
		std::string side_number = str_cast(t.side());
		if (std::find(list.begin(),list.end(),side_number)==list.end())
		{
			return false;
		}
		return true;
}

bool side_filter::match_internal(const team &t) const
{
	if (cfg_.has_attribute("side_in")) {
		if (!check_side_number(t,cfg_["side_in"])) {
			return false;
		}
	}
	if (cfg_.has_attribute("side")) {
		if (!check_side_number(t,cfg_["side"])) {
			return false;
		}
	}
	if (!side_string_.empty()) {
		if (!check_side_number(t,side_string_)) {
			return false;
		}
	}

	config::attribute_value cfg_team_name = cfg_["team_name"];
	if (!cfg_team_name.blank()) {
		const std::string& that_team_name = cfg_team_name;
		const std::string& this_team_name = t.team_name();

		if(std::find(this_team_name.begin(), this_team_name.end(), ',') == this_team_name.end()) {
			if(this_team_name != that_team_name) return false;
		}
		else {
			const std::vector<std::string>& these_team_names = utils::split(this_team_name);
			bool search_futile = true;
			foreach(const std::string& this_single_team_name, these_team_names) {
				if(this_single_team_name == that_team_name) {
					search_futile = false;
					break;
				}
			}
			if(search_futile) return false;
		}
	}

	//Allow filtering on units
	if(cfg_.has_child("has_unit")) {
		const vconfig& unit_filter = cfg_.child("has_unit");
		bool found = false;
		for (unit_map::iterator u = resources::units->begin(); u != resources::units->end(); ++ u) {
			if (u->side() != t.side()) {
				continue;
			}
			if (u->matches_filter(unit_filter, u->get_location(), flat_)) {
				found = true;
				break;
			}
		}

		if (!found) {
			return false;
		}
	}

	const vconfig& enemy_of = cfg_.child("enemy_of");
	if(!enemy_of.null()) {
		side_filter s_filter(enemy_of);
		const std::vector<int>& teams = s_filter.get_teams();
		if(teams.empty()) return false;
		foreach(const int side, teams) {
			if(!(*resources::teams)[side - 1].is_enemy(t.side()))
				return false;
		}
	}

	const vconfig& allied_with = cfg_.child("allied_with");
	if(!allied_with.null()) {
		side_filter s_filter(allied_with);
		const std::vector<int>& teams = s_filter.get_teams();
		if(teams.empty()) return false;
		foreach(const int side, teams) {
			if((*resources::teams)[side - 1].is_enemy(t.side()))
				return false;
		}
	}

	return true;
}

bool side_filter::match(int side) const
{
	return this->match((*resources::teams)[side-1]);
}

bool side_filter::match(const team& t) const
{
	bool matches = match_internal(t);

	//handle [and], [or], and [not] with in-order precedence
	vconfig::all_children_iterator cond = cfg_.ordered_begin();
	vconfig::all_children_iterator cond_end = cfg_.ordered_end();
	while (cond != cond_end) {
		const std::string& cond_name = cond.get_key();
		const vconfig& cond_cfg = cond.get_child();

		//handle [and]
		if(cond_name == "and")
		{
			matches = matches && side_filter(cond_cfg, flat_).match(t);
		}
		//handle [or]
		else if(cond_name == "or")
		{
			matches = matches || side_filter(cond_cfg, flat_).match(t);
		}
		//handle [not]
		else if(cond_name == "not")
		{
			matches = matches && !side_filter(cond_cfg, flat_).match(t);
		}
			++cond;
	}
	return matches;
}