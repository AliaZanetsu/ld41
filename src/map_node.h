/*
 *  Copyright (C) 2018 the authors (see AUTHORS)
 *
 *  This file is part of Draklia's ld41.
 *
 *  lair is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  lair is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with lair.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef LD41_MAP_NODE_H_
#define LD41_MAP_NODE_H_


#include <lair/core/lair.h>

#include "text_moba.h"


class MapNode : public std::enable_shared_from_this<MapNode> {
public:
	typedef std::unordered_map<MapNode*, StringVector> NodeMap;

public:
	const lair::String& id() const;
	const lair::String& name() const;

	const NodeMap& paths() const;
	MapNodeSP destination(const lair::String& direction) const;

	const lair::Path& image() const;
	const lair::Vector2& pos() const;

	const lair::String& tower() const;
	const lair::String& fonxus() const;

	const CharacterSet& characters() const;
	void addCharacter(CharacterSP character);
	void removeCharacter(CharacterSP character);

public:
	lair::String  _id;
	lair::String  _name;
	NodeMap       _paths;
	lair::Path    _image;
	lair::Vector2 _pos;
	lair::String  _tower;
	lair::String  _fonxus;

	CharacterSet  _characters;
};


#endif