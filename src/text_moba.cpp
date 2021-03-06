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

#include "game.h"
#include "main_state.h"
#include "console.h"
#include "commands.h"

#include "map_node.h"
#include "character_class.h"
#include "character.h"
#include "ai.h"
#include "redshirt_ai.h"
#include "tower_ai.h"
#include "tm_command.h"

#include "text_moba.h"


using namespace lair;


// TODO: Move this to Lair


const Variant& getVarItem(const Variant& var, const String& key, bool* success = nullptr) {
	if(!var.isVarMap()) {
		dbgLogger.error("Expected VarMap for key \"", key, "\", got \"", var.type()->identifier, "\".");
		if(success)
			*success = false;
		return Variant::null;
	}

	return var.get(key);
}

bool getBool(const Variant& var, const String& key, bool defaultValue = false, bool* success = nullptr) {
	const Variant& v = getVarItem(var, key, success);

	if(v.isBool()) {
		return v.asBool();
	}
	else if(!v.isNull()) {
		dbgLogger.error("Expected Bool for key \"", key, "\", got \"", var.type()->identifier, "\".");
		if(success)
			*success = false;
	}

	return defaultValue;
}

int64 getInt(const Variant& var, const String& key, int64 defaultValue = 0, bool* success = nullptr) {
	const Variant& v = getVarItem(var, key, success);

	if(v.isInt()) {
		return v.asInt();
	}
	else if(!v.isNull()) {
		dbgLogger.error("Expected Int for key \"", key, "\", got \"", var.type()->identifier, "\".");
		if(success)
			*success = false;
	}

	return defaultValue;
}

float getFloat(const Variant& var, const String& key, float defaultValue = 0, bool* success = nullptr) {
	const Variant& v = getVarItem(var, key, success);

	if(v.isFloat()) {
		return v.asFloat();
	}
	else if(!v.isNull()) {
		dbgLogger.error("Expected Float for key \"", key, "\", got \"", var.type()->identifier, "\".");
		if(success)
			*success = false;
	}

	return defaultValue;
}

const String& getString(const Variant& var, const String& key, const String& defaultValue = String(), bool* success = nullptr) {
	const Variant& v = getVarItem(var, key, success);

	if(v.isString()) {
		return v.asString();
	}
	else if(!v.isNull()) {
		dbgLogger.error("Expected String for key \"", key, "\", got \"", var.type()->identifier, "\".");
		if(success)
			*success = false;
	}

	return defaultValue;
}

IntVector getClassStats(const Variant& var, const String& key, bool* success = nullptr) {
	const Variant& v = getVarItem(var, key, success);

	if(v.isInt()) {
		return IntVector(6, v.asInt());
	}
	if(v.isVarList()) {
		IntVector stats(6, 0);
		unsigned i = 0;
		for(const Variant& v2: v.asVarList()) {
			stats[i] = v2.asInt();
			i += 1;
		}
		return stats;
	}
	else if(!v.isNull()) {
		dbgLogger.error("Expected String.");
		if(success)
			*success = false;
	}

	return IntVector(6, 0);
}



bool CharacterOrder::operator()(const CharacterSP& c0, const CharacterSP& c1) const {
	if(c0->team() < c1->team())
		return true;
	if(c1->team() < c0->team())
		return false;

	if(c0->cClass()->sortIndex() < c1->cClass()->sortIndex())
		return true;
	if(c1->cClass()->sortIndex() < c0->cClass()->sortIndex())
		return false;

	if(c0->index() < c1->index())
		return true;
	return false;
}



const String& teamName(Team team) {
	static const String names[] = {
	    "blue",
	    "red",
	    "neutral",
	};
	return names[team];
}

const String& placeName(Place place) {
	static const String names[] = {
	    "back",
	    "front",
	};
	return names[place];
}

const String& laneName(Lane lane) {
	static const String names[] = {
	    "top",
	    "bot",
	};
	return names[lane];
}

Team enemyTeam(Team team) {
	return Team(team ^ 0x01);
}


unsigned placeIndex(Team team, Place place) {
	return (team == BLUE)?
	            ((place == BACK)?  0: 1):
	            ((place == FRONT)? 2: 3);
}


Team teamFromPlaceIndex(unsigned pi) {
	return Team(pi / 2);
}


Place placeFromPlaceIndex(unsigned pi) {
	Team team = teamFromPlaceIndex(pi);
	unsigned place = pi & 0x01;
	return Place((team == BLUE)? place: 1 - place);
}



TextMoba::TextMoba(MainState* mainState, Console* console)
    : _mainState(mainState)
    , _console(console)
{
	using namespace std::placeholders;

	_console->setExecCommand(std::bind(&TextMoba::_execCommand, this, _1));

	_addCommand<HelpCommand>();
	_addCommand<LookCommand>();
	_addCommand<DirectionsCommand>();
	_addCommand<WaitCommand>();
	_addCommand<GoCommand>();
	_addCommand<MoveCommand>();
	_addCommand<AttackCommand>();
}


void TextMoba::initialize(const Path& logicPath) {
	VirtualFile file = _mainState->game()->fileSystem()->file(logicPath);

	Path realPath = file.realPath();
	if(!realPath.empty()) {
		Path::IStream in(realPath.native().c_str());
		_initialize(in, logicPath);
	}

	const MemFile* memFile = file.fileBuffer();
	if(memFile) {
		String buffer((const char*)memFile->data, memFile->size);
		std::istringstream in(buffer);
		_initialize(in, logicPath);
	}
}


MainState* TextMoba::mainState() {
	return _mainState;
}


Console* TextMoba::console() {
	return _console;
}


MapNodeSP TextMoba::mapNode(const String& id) {
	auto it = _nodes.find(id);
	if(it == _nodes.end())
		return nullptr;
	return it->second;
}


MapNodeSP TextMoba::fonxus(Team team) {
	return mapNode((team == BLUE)? "bf": "rf");
}


CharacterClassSP TextMoba::characterClass(const lair::String& id) {
	auto it = _classes.find(id);
	if(it == _classes.end())
		return nullptr;
	return it->second;
}


CharacterSP TextMoba::player() {
	return _player;
}


CharacterSP TextMoba::spawnCharacter(const lair::String& className, Team team,
                                     MapNodeSP node) {
	CharacterClassSP cc = characterClass(className);
	if(!cc) {
		dbgLogger.error("Invalid character class: \"", className, "\"");
		return CharacterSP();
	}

	CharacterSP character = std::make_shared<Character>(this, cc, _charIndex);
	character->_team = team;

	if(node) {
		moveCharacter(character, node);
	}

	dbgLogger.log("Spawn ", character->teamName(), " ", character->className(),
	              " ", character->index(), " at ", node? node->name(): "<nowhere>");

	_characters.emplace(character);
	++_charIndex;

	return character;
}


CharacterSP TextMoba::spawnRedshirt(Team team, Lane lane) {
	static const String classes[] = {
	    "blueshirt",
	    "redshirt",
	};

	MapNodeSP fonxus = mapNode((team == BLUE)? "bf": "rf");
	CharacterSP redshirt = spawnCharacter(classes[team], team, fonxus);
	redshirt->setAi<RedshirtAi>(lane);
	dbgLogger.info("  RedshirtAi: ", lane);
	return redshirt;
}


void TextMoba::spawnRedshirts(unsigned count) {
	for(unsigned i = 0; i < count; ++i) {
		spawnRedshirt(BLUE, TOP);
		spawnRedshirt(BLUE, BOT);
		spawnRedshirt(RED,  TOP);
		spawnRedshirt(RED,  BOT);
	}
}


void TextMoba::killCharacter(CharacterSP character) {
	dbgLogger.log("Kill ", character->debugName());
	if(character->cClass()->playable()) {
		moveCharacter(character, fonxus(character->team()));
		// TODO: Death & respawn
		character->_hp = character->maxHP();
	}
	else {
		if(character->node()) {
			character->node()->removeCharacter(character);
			character->_node = nullptr;
		}
		_characters.erase(character);
	}
}


void TextMoba::moveCharacter(CharacterSP character, MapNodeSP dest) {
	if(player() && character != player() && player()->isAlive()
	        && !character->cClass()->building()
	        && character->node() == player()->node()) {
		print(character->name(), " leaves the area.");
	}

	if(character->node()) {
		character->node()->removeCharacter(character);
	}

	character->_node = dest;
	character->_place = character->cClass()->defaultPlace();

	if(player() && character != player() && player()->isAlive()
	        && !character->cClass()->building()
	        && character->node() == player()->node()) {
		print(character->name(false), " enters the area.");
	}

	if(dest) {
		dest->addCharacter(character);
	}
}


void TextMoba::placeCharacter(CharacterSP character, Place place) {
//	if(player() && character != player() && player()->isAlive()
//	        && character->node() == player()->node()) {
	    print(character->name(), " moves to the ", placeName(place), " row.");
//	}
	character->_place = place;
}


void TextMoba::nextTurn() {
	_turn += 1;

	for(CharacterSP c: _characters) {
		if(c->ai()) {
			c->ai()->play();
		}
	}

	_nextWaveCounter -= 1;
	if(_nextWaveCounter == 0) {
		_console->writeLine("A new batch of redshirts leave the fonxus.");
		spawnRedshirts(_redshirtPerLane);
		_nextWaveCounter = _waveTime;
	}

	_console->writeLine(cat("End of turn ", _turn));
	_execCommand("look");
}


const TextMoba::TMCommandList& TextMoba::commands() const {
	return _commands;
}


TMCommand* TextMoba::command(const lair::String& name) const {
	auto it = _commandMap.find(name);
	if(it == _commandMap.end())
		return nullptr;
	return it->second;
}


void TextMoba::_addCommand(TMCommandSP command) {
	_commands.emplace_back(command);

	for(const String& id: command->names()) {
		_commandMap.emplace(id, command.get());
		dbgLogger.info("Register command \"", id, "\"");
	}
}


bool TextMoba::_execCommand(const String& command) {
	dbgLogger.log("Exec: ", command);

	StringVector args;

	auto it  = command.begin();
	auto end = command.end();

	while(it != end && std::isspace(*it))
		++it;

	while(it != end) {
		auto argBegin = it;
		while(it != end && !std::isspace(*it))
			++it;

		args.emplace_back(String(argBegin, it));

		while(it != end && std::isspace(*it))
			++it;
	}

	if(args.empty())
		return false;

	TMCommand* tmCommand = this->command(args[0]);

	if(!tmCommand) {
		console()->writeLine(cat("Command \"", args[0], "\" do not exists. Type \"h\" for help."));
	}
	else {
		tmCommand->exec(args);
	}

	return true;
}


void TextMoba::_initialize(std::istream& in, const lair::Path& logicPath) {
	ErrorList errors;
	LdlParser parser(&in, logicPath.utf8String(), &errors, LdlParser::CTX_MAP);

	Variant config;
	if(!ldlRead(parser, config)) {
		dbgLogger.error("Failed to load gameplay data from \"",
		                logicPath.utf8String(), "\"");
		errors.log(dbgLogger);
	}
	errors.log(dbgLogger);

	Variant motd = config.get("motd");
	if(motd.isString()) {
		_console->writeLine(motd.asString());
	}

	_firstWaveTime   = getInt(config, "first_wave_time");
	_waveTime        = getInt(config, "wave_time");
	_redshirtPerLane = getInt(config, "redshirt_per_lane");

	Variant nodes = config.get("nodes");
	if(nodes.isVarMap()) {
		for(const auto& pair: nodes.asVarMap()) {
			const String& id = pair.first;
			const Variant& obj = pair.second;

			MapNodeSP node = std::make_shared<MapNode>();

			node->_id = id;

			const Variant& nameVar = obj.get("name");
			if(nameVar.isString())
				node->_name = nameVar.asString();
			else
				dbgLogger.error("Node without name");

			const Variant& imageVar = obj.get("image");
			if(imageVar.isString())
				node->_image = imageVar.asString();
			else
				dbgLogger.error("Node without image");

			const Variant& posVar = obj.get("position");
			if(posVar.isVarList() && posVar.asVarList().size() == 2) {
				const VarList& pos = posVar.asVarList();
				node->_pos = Vector2(pos[0].asFloat(), pos[1].asFloat());
			}
			else
				dbgLogger.error("Node without position");

			node->_tower  = getString(obj, "tower");
			node->_fonxus = getString(obj, "fonxus");

			_nodes.emplace(node->id(), node);
		}
	}
	else {
		dbgLogger.error("Expected \"nodes\" VarMap.");
	}

	Variant paths = config.get("paths");
	if(paths.isVarList()) {
		for(const Variant& path: paths.asVarList()) {
			const Variant& fromVar     = path.get("from");
			const Variant& toVar       = path.get("to");
			const Variant& fromDirsVar = path.get("from_dirs");
			const Variant& toDirsVar   = path.get("to_dirs");
			if(fromVar.isString() && toVar.isString() &&
			        fromDirsVar.isVarList() && toDirsVar.isVarList()) {
				MapNodeSP from = mapNode(fromVar.asString());
				MapNodeSP to   = mapNode(toVar.asString());

				StringVector& fromDirs =
				        from->_paths.emplace(to.get(),   StringVector()).first->second;
				StringVector& toDirs =
				        to  ->_paths.emplace(from.get(), StringVector()).first->second;

				for(const Variant& dirVar: fromDirsVar.asVarList()) {
					if(dirVar.isString()) {
						fromDirs.emplace_back(dirVar.asString());
					}
				}

				for(const Variant& dirVar: toDirsVar.asVarList()) {
					if(dirVar.isString()) {
						toDirs.emplace_back(dirVar.asString());
					}
				}
			}
			else {
				dbgLogger.error("Invalid path.");
			}
		}
	}
	else {
		dbgLogger.error("Expected \"paths\" VarList.");
	}

	Variant classes = config.get("classes");
	if(classes.isVarMap()) {
		for(const auto& pair: classes.asVarMap()) {
			const String& id = pair.first;
			const Variant& obj = pair.second;

			CharacterClassSP cClass = std::make_shared<CharacterClass>();

			cClass->_id        = id;
			cClass->_name      = getString(obj, "name", "<fixme_no_name>");
			cClass->_sortIndex = getInt(obj, "sort_index", 9999);
			cClass->_playable  = getBool(obj, "playable", false);
			cClass->_building  = getBool(obj, "building", false);

			String defaultPlace = getString(obj, "default_place", "back");
			cClass->_defaultPlace = (defaultPlace == "back")? BACK: FRONT;

			cClass->_maxHP     = getClassStats(obj, "max_hp");
			cClass->_maxMana   = getClassStats(obj, "max_mana");
			cClass->_xp        = getClassStats(obj, "xp");
			cClass->_damage    = getClassStats(obj, "damage");
			cClass->_range     = getClassStats(obj, "range");
			cClass->_speed     = getClassStats(obj, "speed");

			_classes.emplace(cClass->id(), cClass);
		}
	}
	else {
		dbgLogger.error("Expected \"classes\" VarMap.");
	}

	// Setup


	_turn = 0;
	_nextWaveCounter = _firstWaveTime;

	// Player *must* have charIndex 0
	_charIndex = 0;
	_player = spawnCharacter("ranger", BLUE, mapNode("bf"));

	for(const auto& pair: _nodes) {
		MapNodeSP node = pair.second;

		if(node->fonxus().size()) {
			spawnCharacter("fonxus", (node->fonxus() == "blue")? BLUE: RED, node);
		}
		if(node->tower().size()) {
			CharacterSP tower = spawnCharacter("tower", (node->tower() == "blue")? BLUE: RED, node);
			tower->setAi<TowerAi>();
		}
	}

	// Starts the game with a description of the environement
	_execCommand("look");
}
