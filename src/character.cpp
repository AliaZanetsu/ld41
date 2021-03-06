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


#include <lair/core/log.h>

#include "map_node.h"
#include "character_class.h"

#include "character.h"


using namespace lair;


Character::Character(TextMoba* textMoba, CharacterClassSP cClass, unsigned index, unsigned level)
    : _textMoba(textMoba)
    , _cClass(cClass)
    , _index(index)
    , _node(nullptr)
    , _team(BLUE)
    , _place(cClass->defaultPlace())
    , _level(level)
    , _xp(0)
    , _hp(cClass->maxHP(level))
    , _mana(cClass->maxMana(level))
    , _deathTime(0)
{
}


CharacterClassSP Character::cClass() const {
	return _cClass;
}


const String& Character::className() const {
	return _cClass->name();
}


unsigned Character::index() const {
	return _index;
}


String Character::name(bool showIndex) const {
	if(isPlayer()) {
		return "you";
	}

	if(showIndex && _node) {
		return cat(teamName(), " ", className(), " ",
		           _node->characterIndex(shared_from_this()));
	}

	return cat(teamName(), " ", className());
}


String Character::debugName() const {
	return cat(index(), ":", className(),
	           " [", node()? node()->id(): "<nowhere>", ":", placeName(), "]");
}


String Character::shortDesc() const {
	return cat("[", placeName(),
	           "] a level ", level() + 1, " ",
	           teamName(), " ", className(), " (",
	           hp(), " / ", maxHP(), ")");
}


MapNodeSP Character::node() const {
	return _node;
}


Team Character::team() const {
	return _team;
}


Team Character::enemyTeam() const {
	return ::enemyTeam(team());
}


const String& Character::teamName() const {
	return ::teamName(_team);
}


bool Character::isPlayer() const {
	return this == _textMoba->player().get();
}


Place Character::place() const {
	return _place;
}


const String& Character::placeName() const {
	return ::placeName(_place);
}


unsigned Character::maxHP() const {
	return _cClass->maxHP(_level);
}


unsigned Character::maxMana() const {
	return _cClass->maxMana(_level);
}


unsigned Character::level() const {
	return _level;
}


unsigned Character::xp() const {
	return _xp;
}


unsigned Character::hp() const {
	return _hp;
}


unsigned Character::mana() const {
	return _mana;
}


unsigned Character::damage() const {
	return _cClass->damage(_level);
}


unsigned Character::range() const {
	return _cClass->range(_level);
}


bool Character::isAlive() const {
	return _hp > 0;
}


bool Character::deathTime() const {
	return _deathTime;
}


AiSP Character::ai() const {
	return _ai;
}


unsigned Character::placeIndex() const {
	return ::placeIndex(team(), place());
}


void Character::moveTo(MapNodeSP dest) {
	_textMoba->moveCharacter(shared_from_this(), dest);
}


void Character::attack(CharacterSP target) {
	// Save this because it won't be available if target is dead.
	String targetName = target->name();

	unsigned damage = this->damage();
	target->takeDamage(damage);

	dbgLogger.info(debugName(), " attack ", target->debugName(),
	               " for ", damage, " damage");

	if(node() == _textMoba->player()->node()) {
		_textMoba->print(name(), " attack ", targetName,
		                 " for ", damage, " damage");
		if(!target->isAlive()) {
			_textMoba->print(name(), " killed ", targetName);
		}
	}
}


void Character::takeDamage(unsigned damage) {
	if(damage >= hp()) {
		_hp = 0;
		_textMoba->killCharacter(shared_from_this());
	}
	else {
		_hp -= damage;
	}
}
