/*
 * ArcEmu MMORPG Server
 * Copyright (C) 2005-2007 Ascent Team <http://www.ascentemu.com/>
 * Copyright (C) 2008-2009 <http://www.ArcEmu.org/>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "StdAfx.h"
#ifndef WIN32
    #include <dlfcn.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <cstdlib>
    #include <cstring>
#endif

#include <svn_revision.h>
#define SCRIPTLIB_HIPART(x) ((x >> 16))
#define SCRIPTLIB_LOPART(x) ((x & 0x0000ffff))
#define SCRIPTLIB_VERSION_MINOR (BUILD_REVISION % 1000)
#define SCRIPTLIB_VERSION_MAJOR (BUILD_REVISION / 1000)

initialiseSingleton(ScriptMgr);
initialiseSingleton(HookInterface);

ScriptMgr::ScriptMgr()
{
	DefaultGossipScript = new GossipScript();
}

ScriptMgr::~ScriptMgr()
{

}


float ScriptMgr::CalculateDistance(float x1, float y1, float z1, float x2, float y2, float z2)
{
	float dx = x1 - x2;
	float dy = y1 - y2;
	float dz = z1 - z2;
	return sqrt(dx*dx + dy*dy + dz*dz);
}

#ifdef WIN32
uint32 ScriptMgr::LoadScript(string &full_path)
{
	HMODULE mod = LoadLibrary( full_path.c_str() );
	//printf( "  %s : 0x%p : ", full_path.c_str(), reinterpret_cast< uint32* >( mod ));
	if( mod == 0 )
	{
		printf( "error!\n" );
	}
	else
	{
		// find version import
		exp_get_version vcall = (exp_get_version)GetProcAddress(mod, "_exp_get_version");
		exp_script_register rcall = (exp_script_register)GetProcAddress(mod, "_exp_script_register");
		exp_get_script_type scall = (exp_get_script_type)GetProcAddress(mod, "_exp_get_script_type");
		if(vcall == 0 || rcall == 0 || scall == 0)
		{
			printf("version functions not found!\n");
			FreeLibrary(mod);
		}
		else
		{
			uint32 version = vcall();
			uint32 stype = scall();
			if(SCRIPTLIB_LOPART(version) == SCRIPTLIB_VERSION_MINOR && SCRIPTLIB_HIPART(version) == SCRIPTLIB_VERSION_MAJOR)
			{
				_handles.push_back(((SCRIPT_MODULE)mod));
				//printf("v%u.%u : ", SCRIPTLIB_HIPART(version), SCRIPTLIB_LOPART(version));
				rcall(this);
				//printf("loaded.\n");
				return 1;
			}
			else
			{
				FreeLibrary(mod);
				printf("version mismatch!\n");						
			}
		}
	}
	return 0;
}

uint32 ScriptMgr::LoadScript(string &full_path, vector< ScriptingEngine > &ScriptEngines, WIN32_FIND_DATA &data)
{
	HMODULE mod = LoadLibrary( full_path.c_str() );
	printf( "  %s : 0x%p : ", data.cFileName, reinterpret_cast< uint32* >( mod ));
	if( mod == 0 )
	{
		printf( "error!\n" );
	}
	else
	{
		// find version import
		exp_get_version vcall = (exp_get_version)GetProcAddress(mod, "_exp_get_version");
		exp_script_register rcall = (exp_script_register)GetProcAddress(mod, "_exp_script_register");
		exp_get_script_type scall = (exp_get_script_type)GetProcAddress(mod, "_exp_get_script_type");
		if(vcall == 0 || rcall == 0 || scall == 0)
		{
			printf("version functions not found!\n");
			FreeLibrary(mod);
		}
		else
		{
			uint32 version = vcall();
			uint32 stype = scall();
			if(SCRIPTLIB_LOPART(version) == SCRIPTLIB_VERSION_MINOR && SCRIPTLIB_HIPART(version) == SCRIPTLIB_VERSION_MAJOR)
			{
				if( stype & SCRIPT_TYPE_SCRIPT_ENGINE )
				{
					printf("v%u.%u : ", SCRIPTLIB_HIPART(version), SCRIPTLIB_LOPART(version));
					printf("delayed load.\n");
					ScriptingEngine se;
					se.Handle = mod;
					se.InitializeCall = rcall;
					se.Type = stype;

					ScriptEngines.push_back( se );
				}
				else
				{
					_handles.push_back(((SCRIPT_MODULE)mod));
					printf("v%u.%u : ", SCRIPTLIB_HIPART(version), SCRIPTLIB_LOPART(version));
					rcall(this);
					printf("loaded.\n");						
				}

				return 1;
			}
			else
			{
				FreeLibrary(mod);
				printf("version mismatch!\n");						
			}
		}
	}
	return 0;
}
#endif

void ScriptMgr::LoadScripts()
{
	if(!HookInterface::getSingletonPtr())
		new HookInterface;

	string start_path = Config.MainConfig.GetStringDefault( "Script", "BinaryLocation", "script_bin" ) + "\\";
	string search_path = start_path + "*.";

	vector< ScriptingEngine > ScriptEngines;

	/* Loading system for win32 */
#ifdef WIN32

	/* Load Battlegrounds DLL */
	Log.Notice("Server","Loading Battlegrounds...");
	string bg_path = start_path + "..\\Battlegrounds.dll";
	if (LoadScript(bg_path) > 0)
		Log.Notice("Server","Battlegrounds Loaded.");
	else
		Log.Notice("Server","Battlegrounds module did not loaded.");

	Log.Notice("Server","Loading External Script Libraries...");
	sLog.outString("");

	search_path += "dll";

	WIN32_FIND_DATA data;
	uint32 count = 0;
	HANDLE find_handle = FindFirstFile( search_path.c_str(), &data );
	if(find_handle == INVALID_HANDLE_VALUE)
		sLog.outError( "  No external scripts found! Server will continue to function with limited functionality." );
	else
	{
		do
		{
			string full_path = start_path + data.cFileName;
			count += LoadScript(full_path, ScriptEngines, data);
		}
		while(FindNextFile(find_handle, &data));
		FindClose(find_handle);
		sLog.outString("");
		Log.Notice("Server","Loaded %u external libraries.", count);
		sLog.outString("");

		Log.Notice("Server","Loading optional scripting engines...");
		for(vector<ScriptingEngine>::iterator itr = ScriptEngines.begin(); itr != ScriptEngines.end(); ++itr)
		{
			if( itr->Type & SCRIPT_TYPE_SCRIPT_ENGINE_LUA )
			{
				// lua :O
				if( sWorld.m_LuaEngine )
				{
					Log.Notice("Server","Initializing LUA script engine...");
					itr->InitializeCall(this);
					_handles.push_back( (SCRIPT_MODULE)itr->Handle );
				}
				else
				{
					FreeLibrary( itr->Handle );
				}
			}
			else
			{
				Log.Notice("Server","Unknown script engine type: 0x%.2X, please contact developers.", (*itr).Type );
				FreeLibrary( itr->Handle );
			}
		}
		Log.Notice("Server","Done loading script engines...");
	}
#else
	/* Loading system for *nix */
	struct dirent ** list = NULL;
	int filecount = scandir(PREFIX "/lib/", &list, 0, 0);
	uint32 count = 0;

	if(!filecount || !list || filecount < 0)
		sLog.outError("  No external scripts found! Server will continue to function with limited functionality.");
	else
	{
char *ext;
		while(filecount--)
		{
			ext = strrchr(list[filecount]->d_name, '.');
#ifdef HAVE_DARWIN
			if (ext != NULL && strstr(list[filecount]->d_name, ".0.dylib") == NULL && !strcmp(ext, ".dylib")) {
#else
                        if (ext != NULL && !strcmp(ext, ".so")) {
#endif
				string full_path = "../lib/" + string(list[filecount]->d_name);
				SCRIPT_MODULE mod = dlopen(full_path.c_str(), RTLD_NOW);
				printf("  %s : 0x%p : ", list[filecount]->d_name, mod);
				if(mod == 0)
					printf("error! [%s]\n", dlerror());
				else
				{
					// find version import
					exp_get_version vcall = (exp_get_version)dlsym(mod, "_exp_get_version");
					exp_script_register rcall = (exp_script_register)dlsym(mod, "_exp_script_register");
					exp_get_script_type scall = (exp_get_script_type)dlsym(mod, "_exp_get_script_type");
					if(vcall == 0 || rcall == 0 || scall == 0)
					{
						printf("version functions not found!\n");
						dlclose(mod);
					}
					else
					{
						int32 version = vcall();
						uint32 stype = scall();
						if(SCRIPTLIB_LOPART(version) == SCRIPTLIB_VERSION_MINOR && SCRIPTLIB_HIPART(version) == SCRIPTLIB_VERSION_MAJOR)
						{
							if( stype & SCRIPT_TYPE_SCRIPT_ENGINE )
							{
								printf("v%u.%u : ", SCRIPTLIB_HIPART(version), SCRIPTLIB_LOPART(version));
								printf("delayed load.\n");

								ScriptingEngine se;
								se.Handle = mod;
								se.InitializeCall = rcall;
								se.Type = stype;

								ScriptEngines.push_back( se );
							}
							else
							{
								_handles.push_back(((SCRIPT_MODULE)mod));
								printf("v%u.%u : ", SCRIPTLIB_HIPART(version), SCRIPTLIB_LOPART(version));
								rcall(this);
								printf("loaded.\n");						
							}

							++count;
						}
						else
						{
							dlclose(mod);
							printf("version mismatch!\n");						
						}
					}
				}
			}
			free(list[filecount]);
		}
		free(list);
		sLog.outString("");
		sLog.outString("Loaded %u external libraries.", count);
		sLog.outString("");

		sLog.outString("Loading optional scripting engines...");
		for(vector<ScriptingEngine>::iterator itr = ScriptEngines.begin(); itr != ScriptEngines.end(); ++itr)
		{
			if( itr->Type & SCRIPT_TYPE_SCRIPT_ENGINE_LUA )
			{
				// lua :O
				if( sWorld.m_LuaEngine )
				{
					sLog.outString("   Initializing LUA script engine...");
					itr->InitializeCall(this);
					_handles.push_back( (SCRIPT_MODULE)itr->Handle );
				}
				else
				{
					dlclose( itr->Handle );
				}
			}
			else
			{
				sLog.outString("  Unknown script engine type: 0x%.2X, please contact developers.", (*itr).Type );
				dlclose( itr->Handle );
			}
		}
		sLog.outString("Done loading script engines...");
	}
#endif
}

void ScriptMgr::UnloadScripts()
{
	if(HookInterface::getSingletonPtr())
		delete HookInterface::getSingletonPtr();

	for(CustomGossipScripts::iterator itr = _customgossipscripts.begin(); itr != _customgossipscripts.end(); ++itr)
		(*itr)->Destroy();
	_customgossipscripts.clear();
	delete this->DefaultGossipScript;
	this->DefaultGossipScript=NULL;

	LibraryHandleMap::iterator itr = _handles.begin();
	for(; itr != _handles.end(); ++itr)
	{
#ifdef WIN32
		FreeLibrary(((HMODULE)*itr));
#else
		dlclose(*itr);
#endif
	}
	_handles.clear();
}

void ScriptMgr::register_creature_script(uint32 entry, exp_create_creature_ai callback)
{
	_creatures.insert( CreatureCreateMap::value_type( entry, callback ) );
}

void ScriptMgr::register_gameobject_script(uint32 entry, exp_create_gameobject_ai callback)
{
	_gameobjects.insert( GameObjectCreateMap::value_type( entry, callback ) );
}

void ScriptMgr::register_dummy_aura(uint32 entry, exp_handle_dummy_aura callback)
{
	_auras.insert( HandleDummyAuraMap::value_type( entry, callback ) );
}

void ScriptMgr::register_dummy_spell(uint32 entry, exp_handle_dummy_spell callback)
{
	_spells.insert( HandleDummySpellMap::value_type( entry, callback ) );
}

void ScriptMgr::register_gossip_script(uint32 entry, GossipScript * gs)
{
	CreatureInfo * ci = CreatureNameStorage.LookupEntry(entry);
	if(ci)
		ci->gossip_script = gs;

	_customgossipscripts.insert(gs);
}

void ScriptMgr::register_go_gossip_script(uint32 entry, GossipScript * gs)
{
	GameObjectInfo * gi = GameObjectNameStorage.LookupEntry(entry);
	if(gi)
		gi->gossip_script = gs;

	_customgossipscripts.insert(gs);
}

void ScriptMgr::register_quest_script(uint32 entry, QuestScript * qs)
{
	Quest * q = QuestStorage.LookupEntry( entry );
	if( q != NULL )
		q->pQuestScript = qs;

	_questscripts.insert( qs );
}

CreatureAIScript* ScriptMgr::CreateAIScriptClassForEntry(Creature* pCreature)
{
	CreatureCreateMap::iterator itr = _creatures.find(pCreature->GetEntry());
	if(itr == _creatures.end())
		return NULL;

	exp_create_creature_ai function_ptr = itr->second;
	return (function_ptr)(pCreature);
}

GameObjectAIScript * ScriptMgr::CreateAIScriptClassForGameObject(uint32 uEntryId, GameObject* pGameObject)
{
	GameObjectCreateMap::iterator itr = _gameobjects.find(pGameObject->GetEntry());
	if(itr == _gameobjects.end())
		return NULL;

	exp_create_gameobject_ai function_ptr = itr->second;
	return (function_ptr)(pGameObject);
}

bool ScriptMgr::CallScriptedDummySpell(uint32 uSpellId, uint32 i, Spell* pSpell)
{
	HandleDummySpellMap::iterator itr = _spells.find(uSpellId);
	if(itr == _spells.end())
		return false;

	exp_handle_dummy_spell function_ptr = itr->second;
	return (function_ptr)(i, pSpell);
}

bool ScriptMgr::CallScriptedDummyAura(uint32 uSpellId, uint32 i, Aura* pAura, bool apply)
{
	HandleDummyAuraMap::iterator itr = _auras.find(uSpellId);
	if(itr == _auras.end())
		return false;

	exp_handle_dummy_aura function_ptr = itr->second;
	return (function_ptr)(i, pAura, apply);
}

bool ScriptMgr::CallScriptedItem(Item * pItem, Player * pPlayer)
{
	if(pItem->GetProto() && pItem->GetProto()->gossip_script)
	{
		pItem->GetProto()->gossip_script->GossipHello(pItem,pPlayer,true);
		return true;
	}
	
	return false;
}

void ScriptMgr::register_item_gossip_script(uint32 entry, GossipScript * gs)
{
	ItemPrototype * proto = ItemPrototypeStorage.LookupEntry(entry);
	if(proto)
		proto->gossip_script = gs;

	_customgossipscripts.insert(gs);
}

/* CreatureAI Stuff */
CreatureAIScript::CreatureAIScript(Creature* creature) : _unit(creature)
{

}

void CreatureAIScript::RegisterAIUpdateEvent(uint32 frequency)
{
	//sEventMgr.AddEvent(_unit, &Creature::CallScriptUpdate, EVENT_SCRIPT_UPDATE_EVENT, frequency, 0,0);
	sEventMgr.AddEvent(_unit, &Creature::CallScriptUpdate, EVENT_SCRIPT_UPDATE_EVENT, frequency, 0,EVENT_FLAG_DO_NOT_EXECUTE_IN_WORLD_CONTEXT);
}

void CreatureAIScript::ModifyAIUpdateEvent(uint32 newfrequency)
{
	sEventMgr.ModifyEventTimeAndTimeLeft(_unit, EVENT_SCRIPT_UPDATE_EVENT, newfrequency);
}

void CreatureAIScript::RemoveAIUpdateEvent()
{
	sEventMgr.RemoveEvents(_unit, EVENT_SCRIPT_UPDATE_EVENT);
}

/* GameObjectAI Stuff */

GameObjectAIScript::GameObjectAIScript(GameObject* goinstance) : _gameobject(goinstance)
{

}

void GameObjectAIScript::ModifyAIUpdateEvent(uint32 newfrequency)
{
	sEventMgr.ModifyEventTimeAndTimeLeft(_gameobject, EVENT_SCRIPT_UPDATE_EVENT, newfrequency);
}
 
void GameObjectAIScript::RemoveAIUpdateEvent()
{
	sEventMgr.RemoveEvents(_gameobject, EVENT_SCRIPT_UPDATE_EVENT);
}

void GameObjectAIScript::RegisterAIUpdateEvent(uint32 frequency)
{
	sEventMgr.AddEvent(_gameobject, &GameObject::CallScriptUpdate, EVENT_SCRIPT_UPDATE_EVENT, frequency, 0,0);
}


/* InstanceAI Stuff */

InstanceScript::InstanceScript(MapMgr *instance) : _instance(instance)
{
}

/* QuestScript Stuff */

/* Gossip Stuff*/

GossipScript::GossipScript()
{
	
}

void GossipScript::GossipEnd(Object* pObject, Player* Plr)
{
	Plr->CleanupGossipMenu();
}

bool CanTrainAt(Player * plr, Trainer * trn);
void GossipScript::GossipHello(Object* pObject, Player* Plr, bool AutoSend)
{
	GossipMenu *Menu;
	uint32 TextID = 2;
	Creature * pCreature = (pObject->GetTypeId()==TYPEID_UNIT)?static_cast< Creature* >( pObject ):NULL;
	if(!pCreature)
		return;

	uint32 Text = objmgr.GetGossipTextForNpc(pCreature->GetEntry());
	if(Text != 0)
	{
		GossipText * text = NpcTextStorage.LookupEntry(Text);
		if(text != 0)
			TextID = Text;
	}

	objmgr.CreateGossipMenuForPlayer(&Menu, pCreature->GetGUID(), TextID, Plr);
	
	if (pCreature->isVendor())
		Menu->AddItem(1, Plr->GetSession()->LocalizedWorldSrv(1), 1);
	
	if (pCreature->isTrainer() || pCreature->isProf())
	{
		Trainer *pTrainer = pCreature->GetTrainer();
		if(!pTrainer)
		{
			if(AutoSend)
				Menu->SendTo(Plr);
			return;
		}
		string name = pCreature->GetCreatureInfo()->Name;
		string::size_type pos = name.find(" ");	  // only take first name

		if(pos != string::npos)
			name = name.substr(0, pos);

		if(CanTrainAt(Plr, pTrainer))
			Menu->SetTextID(pTrainer->Can_Train_Gossip_TextId);
		else
			Menu->SetTextID(pTrainer->Cannot_Train_GossipTextId);
        if(pTrainer->TrainerType != TRAINER_TYPE_PET)
		{
			string msg = string(Plr->GetSession()->LocalizedWorldSrv(2));
			if(pTrainer->RequiredClass)
			{
				switch(Plr->getClass())
				{
				case MAGE:
					msg += string(Plr->GetSession()->LocalizedWorldSrv(3));
					break;
				case SHAMAN:
					msg += string(Plr->GetSession()->LocalizedWorldSrv(4));
					break;
				case WARRIOR:
					msg += string(Plr->GetSession()->LocalizedWorldSrv(5));
					break;
				case PALADIN:
					msg += string(Plr->GetSession()->LocalizedWorldSrv(6));
					break;
				case WARLOCK:
					msg += string(Plr->GetSession()->LocalizedWorldSrv(7));
					break;
				case HUNTER:
					msg += string(Plr->GetSession()->LocalizedWorldSrv(8));
					break;
				case ROGUE:
					msg += string(Plr->GetSession()->LocalizedWorldSrv(9));
					break;
				case DRUID:
					msg += string(Plr->GetSession()->LocalizedWorldSrv(10));
					break;
				case PRIEST:
					msg += string(Plr->GetSession()->LocalizedWorldSrv(11));
					break;
				}
				msg += " "+string(Plr->GetSession()->LocalizedWorldSrv(12))+", ";
				msg += name;
				msg += ".";

				Menu->AddItem(3, msg.c_str(), 2);

			}
			else
			{
				msg += string(Plr->GetSession()->LocalizedWorldSrv(12))+", ";
				msg += name;
				msg += ".";

				Menu->AddItem(3, msg.c_str(), 2);
			}
		}
		else
		{
			
			Menu->AddItem(3, Plr->GetSession()->LocalizedWorldSrv(13), 2);
		}

		if (pTrainer->RequiredClass &&					  // class trainer
			pTrainer->RequiredClass == Plr->getClass() &&   // correct class
			pCreature->getLevel() > 10 &&				   // creature level
			pTrainer->TrainerType != TRAINER_TYPE_PET &&  // Pet Trainers do not respec hunter talents
			Plr->getLevel() > 10 )						  // player level
		{
			Menu->AddItem(0, Plr->GetSession()->LocalizedWorldSrv(22), 11);
		}
	
		if (pTrainer->TrainerType == TRAINER_TYPE_PET &&	// pet trainer type
			Plr->getClass() == HUNTER &&					// hunter class
			Plr->GetSummon() != NULL )						// have pet
		{
			Menu->AddItem(0, Plr->GetSession()->LocalizedWorldSrv(23), 13);
		}

	} // end architype trainer / profession trainer

	if (pCreature->isTaxi())
		Menu->AddItem(2, Plr->GetSession()->LocalizedWorldSrv(14), 3);

	if (pCreature->isAuctioner())
		Menu->AddItem(0, Plr->GetSession()->LocalizedWorldSrv(15), 4);

	if (pCreature->isInnkeeper())
		Menu->AddItem(5, Plr->GetSession()->LocalizedWorldSrv(16), 5);

	if (pCreature->isBattleMaster())
		Menu->AddItem(0, Plr->GetSession()->LocalizedWorldSrv(21), 10);

	if (pCreature->isBanker())
		Menu->AddItem(0, Plr->GetSession()->LocalizedWorldSrv(17), 6);

	if (pCreature->isSpiritHealer())
		Menu->AddItem(0, Plr->GetSession()->LocalizedWorldSrv(18), 7);

	if (pCreature->isCharterGiver())
		Menu->AddItem(0, Plr->GetSession()->LocalizedWorldSrv(19), 8);

	if (pCreature->isTabardDesigner())
		Menu->AddItem(0, Plr->GetSession()->LocalizedWorldSrv(20), 9);

	if(AutoSend)
		Menu->SendTo(Plr);
}

void GossipScript::GossipSelectOption(Object* pObject, Player* Plr, uint32 Id, uint32 IntId, const char * EnteredCode)
{
	Creature* pCreature = static_cast< Creature* >( pObject );
	if( pObject->GetTypeId() != TYPEID_UNIT )
		return;

	sLog.outDebug("GossipSelectOption: Id = %u, IntId = %u", Id, IntId);

	switch( IntId )
	{
	case 1:
		// vendor
		Plr->GetSession()->SendInventoryList(pCreature);
		break;
	case 2:
		// trainer
		Plr->GetSession()->SendTrainerList(pCreature);
		break;
	case 3:
		// taxi
		Plr->GetSession()->SendTaxiList(pCreature);
		break;
	case 4:
		// auction
		Plr->GetSession()->SendAuctionList(pCreature);
		break;
	case 5:
		// innkeeper
		Plr->GetSession()->SendInnkeeperBind(pCreature);
		break;
	case 6:
		// banker
		Plr->GetSession()->SendBankerList(pCreature);
		break;
	case 7:
		// spirit
		Plr->GetSession()->SendSpiritHealerRequest(pCreature);
		break;
	case 8:
		// petition
		Plr->GetSession()->SendCharterRequest(pCreature);
		break;
	case 9:
		// guild crest
		Plr->GetSession()->SendTabardHelp(pCreature);
		break;
	case 10:
		// battlefield
		Plr->GetSession()->SendBattlegroundList(pCreature, 0);
		break;
	case 11:
		// switch to talent reset message
		{
			GossipMenu *Menu;
			objmgr.CreateGossipMenuForPlayer(&Menu, pCreature->GetGUID(), 5674, Plr);
			Menu->AddItem(0, Plr->GetSession()->LocalizedWorldSrv(24), 12);
			Menu->SendTo(Plr);
		}break;
	case 12:
		// talent reset
		{
			Plr->Gossip_Complete();
			Plr->SendTalentResetConfirm();
		}break;
	case 13:
		// switch to untrain message
		{
			GossipMenu *Menu;
			objmgr.CreateGossipMenuForPlayer(&Menu, pCreature->GetGUID(), 7722, Plr);
			Menu->AddItem(0, Plr->GetSession()->LocalizedWorldSrv(25), 14);
			Menu->SendTo(Plr);
		}break;
	case 14:
		// untrain pet
		{
			Plr->Gossip_Complete();
			Plr->SendPetUntrainConfirm();
		}break;

	default:
		sLog.outError("Unknown IntId %u on entry %u", IntId, pCreature->GetEntry());
		break;
	}	
}

void GossipScript::Destroy()
{
	delete this;
}

void ScriptMgr::register_hook(ServerHookEvents event, void * function_pointer)
{
	ASSERT(event < NUM_SERVER_HOOKS);
	_hooks[event].push_back(function_pointer);
}

/* Hook Implementations */
bool HookInterface::OnNewCharacter(uint32 Race, uint32 Class, WorldSession * Session, const char * Name)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_NEW_CHARACTER];
	bool ret_val = true;
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
	{
		bool rv = ((tOnNewCharacter)*itr)(Race, Class, Session, Name);
		if (rv == false) // never set ret_val back to true, once it's false
			ret_val = false;
	}
	return ret_val;
}

void HookInterface::OnPlayerKill(Player * pPlayer, Player * pVictim)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_PLAYER_KILL];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnPlayerKill)*itr)(pPlayer, pVictim);
}

void HookInterface::OnFirstEnterWorld(Player * pPlayer)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_FIRST_ENTER_WORLD];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnFirstEnterWorld)*itr)(pPlayer);
}

void HookInterface::OnCharacterCreate(Player * pPlayer)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_CHARACTER_CREATE];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOCharacterCreate)*itr)(pPlayer);
}

void HookInterface::OnEnterWorld(Player * pPlayer)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_ENTER_WORLD];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnEnterWorld)*itr)(pPlayer);
}

void HookInterface::OnGuildCreate(Player * pLeader, Guild * pGuild)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_GUILD_CREATE];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnGuildCreate)*itr)(pLeader, pGuild);
}

void HookInterface::OnGuildJoin(Player * pPlayer, Guild * pGuild)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_GUILD_JOIN];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnGuildJoin)*itr)(pPlayer, pGuild);
}

void HookInterface::OnPlayerDeath(Player * pPlayer)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_DEATH];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnPlayerDeath)*itr)(pPlayer);
}

void HookInterface::OnMount(Player * pPlayer)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_MOUNT];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnMount)*itr)(pPlayer);
}

void HookInterface::OnFlagDropPickup(Player * pPlayer, GameObject * obj)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_FLAG_DROP_PICKUP];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnFlagDropPickup)*itr)(pPlayer, obj);
}

void HookInterface::OnFlagDrop(Player * pPlayer)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_FLAG_DROP];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnFlagDrop)*itr)(pPlayer);
}

void HookInterface::OnHonorKill(Player * pPlayer)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_HONOR_KILL];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnHonorKill)*itr)(pPlayer);
}

void HookInterface::OnAreaTrigger(Player * pPlayer, uint32 id)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_AREA_TRIGGER];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnAreaTrigger)*itr)(pPlayer, id);
}

void HookInterface::OnFlagStandPickup(Player * pPlayer, GameObject * obj)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_FLAG_STAND_PICKUP];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnFlagDropPickup)*itr)(pPlayer, obj);
}

void HookInterface::OnUnitKill(Player * pPlayer, Unit * pVictim)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_UNIT_KILL];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnUnitKill)*itr)(pPlayer, pVictim);
}

void HookInterface::OnDestoryGameObject(GameObject *gameObject)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_DESTROY_GAME_OBJ];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnDestoryGameObject)*itr)(gameObject);
}

bool HookInterface::OnRepopRequest(Player * pPlayer)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_REPOP];
	bool ret_val = false;
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
	{
		bool rv = ((tOnRepopRequest)*itr)(pPlayer);
		if (rv == true) // never set ret_val back to false, once it's true
			ret_val = true;
	}
	return ret_val;
}

void HookInterface::OnEmote(Player * pPlayer, uint32 Emote, Unit * pUnit)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_EMOTE];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnEmote)*itr)(pPlayer, Emote, pUnit);
}

void HookInterface::OnEnterCombat(Player * pPlayer, Unit * pTarget)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_ENTER_COMBAT];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnEnterCombat)*itr)(pPlayer, pTarget);
}

bool HookInterface::OnSpellCast(Player * pPlayer, SpellEntry* pSpell)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_SPELL_CAST];
	bool ret_val = false;
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
	{
		bool rv = ((tOnSpellCast)*itr)(pPlayer, pSpell);
		if (rv == true) // never set ret_val back to false, once it's true
			ret_val = true;
	}
	return ret_val;
}

bool HookInterface::OnLogoutRequest(Player * pPlayer)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_LOGOUT_REQUEST];
	bool ret_val = true;
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
	{
		bool rv = ((tOnLogoutRequest)*itr)(pPlayer);
		if (rv == false) // never set ret_val back to true, once it's false
			ret_val = false;
	}
	return ret_val;
}

void HookInterface::OnLogout(Player * pPlayer)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_LOGOUT];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnLogout)*itr)(pPlayer);
}

void HookInterface::OnQuestAccept(Player * pPlayer, Quest * pQuest, Object * pQuestGiver)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_QUEST_ACCEPT];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnQuestAccept)*itr)(pPlayer, pQuest, pQuestGiver);
}

void HookInterface::OnZone(Player * pPlayer, uint32 zone)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_ZONE];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnZone)*itr)(pPlayer, zone);
}

bool HookInterface::OnChat(Player * pPlayer, uint32 type, uint32 lang, const char * message, const char * misc)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_CHAT];
	bool ret_val = true;
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
	{
		bool rv = ((tOnChat)*itr)(pPlayer, type, lang, message, misc);
		if (rv == false) // never set ret_val back to true, once it's false
			ret_val = false;
	}
	return ret_val;
}

void HookInterface::OnLoot(Player * pPlayer, Unit * pTarget, uint32 money, uint32 itemId)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_LOOT];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnLoot)*itr)(pPlayer, pTarget, money, itemId);
}

void HookInterface::OnObjectLoot(Player * pPlayer, Object * pTarget, uint32 money, uint32 itemId)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_OBJECTLOOT];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnObjectLoot)*itr)(pPlayer, pTarget, money, itemId);
}

void HookInterface::OnEnterWorld2(Player * pPlayer)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_ENTER_WORLD_2];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnEnterWorld)*itr)(pPlayer);
}

void HookInterface::OnQuestCancelled(Player * pPlayer, Quest * pQuest)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_QUEST_CANCELLED];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnQuestCancel)*itr)(pPlayer, pQuest);
}

void HookInterface::OnQuestFinished(Player * pPlayer, Quest * pQuest, Object * pQuestGiver)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_QUEST_FINISHED];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnQuestFinished)*itr)(pPlayer, pQuest, pQuestGiver);
}

void HookInterface::OnHonorableKill(Player * pPlayer, Player * pKilled)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_HONORABLE_KILL];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnHonorableKill)*itr)(pPlayer, pKilled);
}

void HookInterface::OnArenaFinish(Player * pPlayer, ArenaTeam* pTeam, bool victory, bool rated)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_ARENA_FINISH];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnArenaFinish)*itr)(pPlayer, pTeam, victory, rated);
}

void HookInterface::OnPostLevelUp(Player * pPlayer)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_POST_LEVELUP];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnPostLevelUp)*itr)(pPlayer);
}

void HookInterface::OnPreUnitDie(Unit *killer, Unit *victim)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_PRE_DIE];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnPreUnitDie)*itr)(killer, victim);
}

void HookInterface::OnAdvanceSkillLine(Player * pPlayer, uint32 skillLine, uint32 current)
{
	ServerHookList hookList = sScriptMgr._hooks[SERVER_HOOK_EVENT_ON_ADVANCE_SKILLLINE];
	for(ServerHookList::iterator itr = hookList.begin(); itr != hookList.end(); ++itr)
		((tOnAdvanceSkillLine)*itr)(pPlayer, skillLine, current);
}
