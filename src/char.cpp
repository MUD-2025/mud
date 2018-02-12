// $RCSfile: char.cpp,v $     $Date: 2012/01/27 04:04:43 $     $Revision: 1.85 $
// Copyright (c) 2008 Krodo
// Part of Bylins http://www.mud.ru

#include "char.hpp"

#include "world.characters.hpp"
#include "logger.hpp"
#include "obj.hpp"
#include "db.h"
#include "pk.h"
#include "im.h"
#include "handler.h"
#include "interpreter.h"
#include "boards.h"
#include "privilege.hpp"
#include "skills.h"
#include "constants.h"
#include "char_player.hpp"
#include "spells.h"
#include "comm.h"
#include "room.hpp"
#include "player_races.hpp"
#include "celebrates.hpp"
#include "cache.hpp"
#include "fight.h"
#include "house.h"
#include "help.hpp"
#include "utils.h"
#include "msdp.constants.hpp"
#include "backtrace.hpp"

#include <boost/format.hpp>

#include <sstream>
#include <list>
#include <algorithm>
#include <iostream>

std::string PlayerI::empty_const_str;
MapSystem::Options PlayerI::empty_map_options;

namespace
{

// * �� ����������� - ������� �� ��� ������ character.
void check_purged(const CHAR_DATA *ch, const char *fnc)
{
	if (ch->purged())
	{
		log("SYSERR: Using purged character (%s).", fnc);
	}
}

int normalize_skill(int percent)
{
	const static int KMinSkillPercent = 0;
	const static int KMaxSkillPercent = 200;

	if (percent < KMinSkillPercent)
		percent = KMinSkillPercent;
	else if (percent > KMaxSkillPercent)
		percent = KMaxSkillPercent;

	return percent;
}

// ������ ��� �������� ������� �� ����������� ��� ����� ����
// ��� ��������� � ������ ��� �������� � ��� �� ������ ���������
std::list<CHAR_DATA *> fighting_list;

} // namespace

CHAR_DATA::CHAR_DATA() :
	chclass_(CLASS_UNDEFINED),
	role_(MOB_ROLE_TOTAL_NUM),
	in_room(Rooms::UNDEFINED_ROOM_VNUM),
	m_wait(~0u),
	m_master(nullptr),
	proto_script(new OBJ_DATA::triggers_list_t()),
	followers(nullptr)
{
	this->zero_init();
	current_morph_ = GetNormalMorphNew(this);
	caching::character_cache.add(this);
}

CHAR_DATA::~CHAR_DATA()
{
	this->purge();
}

void CHAR_DATA::set_souls(int souls)
{
	this->souls = souls;
}

void CHAR_DATA::inc_souls()
{
	this->souls++;
}

void CHAR_DATA::dec_souls()
{
	this->souls--;
}

int CHAR_DATA::get_souls()
{
	return this->souls;
}

bool CHAR_DATA::in_used_zone() const
{
	if (IS_MOB(this))
	{
		return 0 != zone_table[world[in_room]->zone].used;
	}
	return false;
}

//���������� ������� �� ����� � �����
//P_DAMROLL, P_HITROLL, P_CAST, P_MEM_GAIN, P_MOVE_GAIN, P_HIT_GAIN
float CHAR_DATA::get_cond_penalty(int type) const
{
	if (IS_NPC(this)) return 1;
	if (!(GET_COND_M(this,FULL)||GET_COND_M(this,THIRST))) return 1;
	
	float penalty = 0;
	
	if (GET_COND_M(this,FULL)) {
		int tmp = GET_COND_K(this,FULL); // 0 - 1
		switch (type) {
			case P_DAMROLL://-50%
				penalty+=tmp/2; 
				break;
			case P_HITROLL://-25%
				penalty+=tmp/4; 
				break;
			case P_CAST://-25%
				penalty+=tmp/4; 
				break;
			case P_MEM_GAIN://-25%
				penalty+=tmp/4;
				break;
			case P_MOVE_GAIN://-50%
				penalty+=tmp/2;
				break;
			case P_HIT_GAIN://-50%
				penalty+=tmp/2;
				break;
			case P_AC://-50%
				penalty+=tmp/2;
				break;
			default:
				break;
		}
	}

	if (GET_COND_M(this,THIRST)) {
		int tmp = GET_COND_K(this,THIRST); // 0 - 1
		switch (type) {
			case P_DAMROLL://-25%
				penalty+=tmp/4; 
				break;
			case P_HITROLL://-50%
				penalty+=tmp/2; 
				break;
			case P_CAST://-50%
				penalty+=tmp/2; 
				break;
			case P_MEM_GAIN://-50%
				penalty+=tmp/2;
				break;
			case P_MOVE_GAIN://-25%
				penalty+=tmp/4;
				break;
			case P_AC://-25%
				penalty+=tmp/4;
				break;
			default:
				break;
		}
	}
	penalty=100-MIN(MAX(0,penalty),100);
	penalty/=100.0;
	return penalty;
}

void CHAR_DATA::reset()
{
	int i;

	for (i = 0; i < NUM_WEARS; i++)
	{
		GET_EQ(this, i) = NULL;
	}
	memset((void *)&add_abils, 0, sizeof(add_abils));

	followers = NULL;
	m_master = nullptr;
	in_room = NOWHERE;
	carrying = NULL;
	next_fighting = NULL;
	set_protecting(0);
	set_touching(0);
	BattleAffects = clear_flags;
	Poisoner = 0;
	set_fighting(0);
	char_specials.position = POS_STANDING;
	mob_specials.default_pos = POS_STANDING;
	char_specials.carry_weight = 0;
	char_specials.carry_items = 0;

	if (GET_HIT(this) <= 0)
	{
		GET_HIT(this) = 1;
	}
	if (GET_MOVE(this) <= 0)
	{
		GET_MOVE(this) = 1;
	}

	GET_CASTER(this) = 0;
	GET_DAMAGE(this) = 0;

	PlayerI::reset();
}

void CHAR_DATA::set_abstinent()
{
	int duration = pc_duration(this, 2, MAX(0, GET_DRUNK_STATE(this) - CHAR_DRUNKED), 4, 2, 5);

	if (can_use_feat(this, DRUNKARD_FEAT))
	{
		duration /= 2;
	}

	AFFECT_DATA<EApplyLocation> af;
	af.type = SPELL_ABSTINENT;
	af.bitvector = to_underlying(EAffectFlag::AFF_ABSTINENT);
	af.duration = duration;

	af.location = APPLY_AC;
	af.modifier = 20;
	affect_join(this, af, 0, 0, 0, 0);

	af.location = APPLY_HITROLL;
	af.modifier = -2;
	affect_join(this, af, 0, 0, 0, 0);

	af.location = APPLY_DAMROLL;
	af.modifier = -2;
	affect_join(this, af, 0, 0, 0, 0);
}

void CHAR_DATA::affect_remove(const char_affects_list_t::iterator& affect_i)
{
	int was_lgt = AFF_FLAGGED(this, EAffectFlag::AFF_SINGLELIGHT) ? LIGHT_YES : LIGHT_NO;
	long was_hlgt = AFF_FLAGGED(this, EAffectFlag::AFF_HOLYLIGHT) ? LIGHT_YES : LIGHT_NO;
	long was_hdrk = AFF_FLAGGED(this, EAffectFlag::AFF_HOLYDARK) ? LIGHT_YES : LIGHT_NO;

	if (affected.empty())
	{
		log("SYSERR: affect_remove(%s) when no affects...", GET_NAME(this));
		return;
	}

	const auto af = *affect_i;
	affect_modify(this, af->location, af->modifier, static_cast<EAffectFlag>(af->bitvector), FALSE);
	if (af->type == SPELL_ABSTINENT)
	{
		if (player_specials)
		{
			GET_DRUNK_STATE(this) = GET_COND(this, DRUNK) = MIN(GET_COND(this, DRUNK), CHAR_DRUNKED - 1);
		} else
		{
			log("SYSERR: player_specials is not set.");
		}
	}
	if (af->type == SPELL_DRUNKED && af->duration == 0)
	{
		set_abstinent();
	}

	affected.erase(affect_i);

	affect_total(this);
	check_light(this, LIGHT_UNDEF, was_lgt, was_hlgt, was_hdrk, 1);
}

bool CHAR_DATA::has_any_affect(const affects_list_t& affects)
{
	for (const auto& affect : affects)
	{
		if (AFF_FLAGGED(this, affect))
		{
			return true;
		}
	}

	return false;
}

size_t CHAR_DATA::remove_random_affects(const size_t count)
{
	std::deque<char_affects_list_t::iterator> removable_affects;
	for (auto affect_i = affected.begin(); affect_i != affected.end(); ++affect_i)
	{
		const auto& affect = *affect_i;
		if (affect->removable())
		{
			removable_affects.push_back(affect_i);
		}
	}

	const auto to_remove = std::min(count, removable_affects.size());
	std::random_shuffle(removable_affects.begin(), removable_affects.end());
	for (auto counter = 0u; counter < to_remove; ++counter)
	{
		const auto affect_i = removable_affects[counter];
		affect_remove(affect_i);
	}

	return to_remove;
}

const char* CHAR_DATA::print_affects_to_buffer(char* buffer, const size_t size) const
{
	char* pos = buf;
	size_t remaining = size;

	if (1 > size)
	{
		log("SYSERR: Requested print into buffer of zero size.");
		*buffer = '\0';
		return buffer;
	}

	for (const auto affect : affected)
	{
		const auto written = snprintf(pos, remaining, " %s", apply_types[affect->location]);

		if (written < 0)
		{
			log("SYSERR: Something went wrong while printing list of affects.");
			*buffer = '\0';				// Return empty string
			break;
		}

		if (remaining >= static_cast<size_t>(written))
		{
			log("SYSERR: Provided buffer is not big enough to print list of affects. Truncated.");
			buffer[size - 1] = '\0';	// Terminate string at the end of buffer
			break;
		}

		remaining -= written;
		pos += written;
	}

	return buffer;
}

/**
* ��������� ���� ����� Character (������ ������������),
* �������� � ��������� �������, ����� ������� �� purge().
*/
void CHAR_DATA::zero_init()
{
	protecting_ = 0;
	touching_ = 0;
	fighting_ = 0;
	in_fighting_list_ = 0;
	serial_num_ = 0;
	purged_ = 0;
	// �� �����-�������
	chclass_ = 0;
	level_ = 0;
	idnum_ = 0;
	uid_ = 0;
	exp_ = 0;
	remorts_ = 0;
	last_logon_ = 0;
	gold_ = 0;
	bank_gold_ = 0;
	ruble = 0;
	str_ = 0;
	str_add_ = 0;
	dex_ = 0;
	dex_add_ = 0;
	con_ = 0;
	con_add_ = 0;
	wis_ = 0;
	wis_add_ = 0;
	int_ = 0;
	int_add_ = 0;
	cha_ = 0;
	cha_add_ = 0;
	role_.reset();
	attackers_.clear();
	restore_timer_ = 0;
	// char_data
	nr = NOBODY;
	in_room = 0;
	set_wait(0u);
	punctual_wait = 0;
	last_comm = 0;
	player_specials = 0;
	timed = 0;
	timed_feat = 0;
	carrying = 0;
	desc = 0;
	id = 0;
	proto_script.reset(new OBJ_DATA::triggers_list_t());
	script = 0;
	memory = 0;
	next_fighting = 0;
	followers = 0;
	m_master = nullptr;
	CasterLevel = 0;
	DamageLevel = 0;
	pk_list = 0;
	helpers = 0;
	track_dirs = 0;
	CheckAggressive = 0;
	ExtractTimer = 0;
	Initiative = 0;
	BattleCounter = 0;
	round_counter = 0;
	Poisoner = 0;
	ing_list = 0;
	dl_list = 0;
	agrobd = false;
	memset(&extra_attack_, 0, sizeof(extra_attack_type));
	memset(&cast_attack_, 0, sizeof(cast_attack_type));
	memset(&player_data, 0, sizeof(char_player_data));
	memset(&add_abils, 0, sizeof(char_played_ability_data));
	memset(&real_abils, 0, sizeof(char_ability_data));
	memset(&points, 0, sizeof(char_point_data));
	memset(&char_specials, 0, sizeof(char_special_data));
	memset(&mob_specials, 0, sizeof(mob_special_data));

	for (int i = 0; i < NUM_WEARS; i++)
	{
		equipment[i] = 0;
	}

	memset(&MemQueue, 0, sizeof(spell_mem_queue));
	memset(&Temporary, 0, sizeof(FLAG_DATA));
	memset(&BattleAffects, 0, sizeof(FLAG_DATA));
	char_specials.position = POS_STANDING;
	mob_specials.default_pos = POS_STANDING;
	souls = 0;
}

/**
 * ������������ ���������� � Character ������, �������� �� �����������,
 * �.�. ���� ������������� ������� �������� �� delete.
 * \param destructor - true ����� ��������� �� ����������� � ��������/���������
 * � purged_list �� �����, �� ������� = false.
 *
 * ������� � �����: ���, ��� ��� ��� ���������� � ��������, � �������������� �����
 * ���������� delete - ������ ���� ���������� ����� purge(), � ������� ���� ������
 * ����������� � ������ �������� �������, �� ���� �������� ��� ���� �����������,
 * ���� ������ ���� purged_, ����� ���� �� ���� ����� ��������� ������ ��������� ��
 * ������ ����� ����� purged() �, ��������������, �� ������������ ���, ���� ������
 * �� ����� ��� ������. ����� ������� �� �� ���������� �� ���������� ���������, ����
 * �� ���� ������� ���� �������� ���/���/������. �������� ��-��� ������ � ��������
 * ������� �� ������ � ��������� heartbeat(), ��� ��� � ������ ��������� ��������.
 */
void CHAR_DATA::purge()
{
	caching::character_cache.remove(this);

	if (!get_name().empty())
	{
		log("[FREE CHAR] (%s)", GET_NAME(this));
	}

	int i, j, id = -1;
	struct alias_data *a;

	if (!IS_NPC(this) && !get_name().empty())
	{
		id = get_ptable_by_name(GET_NAME(this));
		if (id >= 0)
		{
			player_table[id].level = GET_LEVEL(this);
			player_table[id].remorts = get_remort();
			player_table[id].activity = number(0, OBJECT_SAVE_ACTIVITY - 1);
		}
	}

	if (!IS_NPC(this) || (IS_NPC(this) && GET_MOB_RNUM(this) == -1))
	{	// if this is a player, or a non-prototyped non-player, free all
		for (j = 0; j < CObjectPrototype::NUM_PADS; j++)
			if (GET_PAD(this, j))
				free(GET_PAD(this, j));

		if (this->player_data.title)
			free(this->player_data.title);

		if (this->player_data.long_descr)
			free(this->player_data.long_descr);

		if (this->player_data.description)
			free(this->player_data.description);

		if (IS_NPC(this) && this->mob_specials.Questor)
			free(this->mob_specials.Questor);

		pk_free_list(this);

		while (this->helpers)
		{
			REMOVE_FROM_LIST(this->helpers, this->helpers, [](auto list) -> auto& { return list->next_helper; });
		}
	}
	else if ((i = GET_MOB_RNUM(this)) >= 0)
	{	// otherwise, free strings only if the string is not pointing at proto
		for (j = 0; j < CObjectPrototype::NUM_PADS; j++)
			if (GET_PAD(this, j)
					&& (this->player_data.PNames[j] != mob_proto[i].player_data.PNames[j]))
				free(this->player_data.PNames[j]);

		if (this->player_data.title && this->player_data.title != mob_proto[i].player_data.title)
			free(this->player_data.title);

		if (this->player_data.long_descr && this->player_data.long_descr != mob_proto[i].player_data.long_descr)
			free(this->player_data.long_descr);

		if (this->player_data.description && this->player_data.description != mob_proto[i].player_data.description)
			free(this->player_data.description);

		if (this->mob_specials.Questor && this->mob_specials.Questor != mob_proto[i].mob_specials.Questor)
			free(this->mob_specials.Questor);
	}

	while (!this->affected.empty())
	{
		affect_remove(affected.begin());
	}

	while (this->timed)
	{
		timed_from_char(this, this->timed);
	}

	Celebrates::remove_from_mob_lists(this->id);

	const bool keep_player_specials = player_specials == player_special_data::s_for_mobiles ? true : false;
	if (this->player_specials && !keep_player_specials)
	{
		while ((a = GET_ALIASES(this)) != NULL)
		{
			GET_ALIASES(this) = (GET_ALIASES(this))->next;
			free_alias(a);
		}
		if (this->player_specials->poofin)
			free(this->player_specials->poofin);
		if (this->player_specials->poofout)
			free(this->player_specials->poofout);
		// �������
		while (GET_RSKILL(this) != NULL)
		{
			im_rskill *r;
			r = GET_RSKILL(this)->link;
			free(GET_RSKILL(this));
			GET_RSKILL(this) = r;
		}
		// �������
		while (GET_PORTALS(this) != NULL)
		{
			struct char_portal_type *prt_next;
			prt_next = GET_PORTALS(this)->next;
			free(GET_PORTALS(this));
			GET_PORTALS(this) = prt_next;
		}
// Cleanup punish reasons
		if (MUTE_REASON(this))
			free(MUTE_REASON(this));
		if (DUMB_REASON(this))
			free(DUMB_REASON(this));
		if (HELL_REASON(this))
			free(HELL_REASON(this));
		if (FREEZE_REASON(this))
			free(FREEZE_REASON(this));
		if (NAME_REASON(this))
			free(NAME_REASON(this));
// End reasons cleanup

		if (KARMA(this))
			free(KARMA(this));

		free(GET_LOGS(this));
// shapirus: ��������� �� ����������� �������� memory leak,
// ��������� ��������������� ������� ������...
		if (EXCHANGE_FILTER(this))
		{
			free(EXCHANGE_FILTER(this));
		}
		EXCHANGE_FILTER(this) = NULL;	// �� ������ ������

		clear_ignores();

		if (GET_CLAN_STATUS(this))
		{
			free(GET_CLAN_STATUS(this));
		}

		// ������ ���� �������
		while (LOGON_LIST(this))
		{
			struct logon_data *log_next;
			log_next = LOGON_LIST(this)->next;
			free(this->player_specials->logons->ip);
			delete LOGON_LIST(this);
			LOGON_LIST(this) = log_next;
		}
		LOGON_LIST(this) = NULL;

		this->player_specials.reset();

		if (IS_NPC(this))
		{
			log("SYSERR: Mob %s (#%d) had player_specials allocated!", GET_NAME(this), GET_MOB_VNUM(this));
		}
	}
	name_.clear();
	short_descr_.clear();
}

// * ����� � ������ ���� ������ � ������� �� ������/���.
int CHAR_DATA::get_skill(const ESkill skill_num) const
{
	int skill = get_trained_skill(skill_num) + get_equipped_skill(skill_num);
	if (AFF_FLAGGED(this, EAffectFlag::AFF_SKILLS_REDUCE))
	{
		skill -= skill * GET_POISON(this) / 100;
	}
	return normalize_skill(skill);
}

// * ����� �� ������.
int CHAR_DATA::get_equipped_skill(const ESkill skill_num) const
{
	int skill = 0;

// ����� � ��� �������, � ������� ����� �������� ������, ��������� ����� � ������ ������ ���������,
// ���� ��������� -- �� ����� 5% � ������
    // ���� ��� ������� ��� ����, ����� �������� �� ����� ���������� �� ����������.
	int is_native = IS_NPC(this) || skill_info[skill_num].classknow[chclass_][(int) GET_KIN(this)] == KNOW_SKILL;
	//int is_native = true;
	for (int i = 0; i < NUM_WEARS; ++i)
	{
		if (equipment[i])
		{
			if (is_native)
			{
				skill += equipment[i]->get_skill(skill_num);
			}
			// �� ����� ��� ��������
			/*else
			{
				skill += (MIN(5, equipment[i]->get_skill(skill_num)));
			}*/
		}
	}
	skill += obj_bonus_.get_skill(skill_num);
	/*if (is_native)
		skill += obj_bonus_.get_skill(skill_num);*/

	return skill;
}

// * ������ ������������� ����� ����.
int CHAR_DATA::get_inborn_skill(const ESkill skill_num)
{
	if (Privilege::check_skills(this))
	{
		CharSkillsType::iterator it = skills.find(skill_num);
		if (it != skills.end())
		{
			return normalize_skill(it->second);
		}
	}
	return 0;
}

int CHAR_DATA::get_trained_skill(const ESkill skill_num) const
{
	if (Privilege::check_skills(this))
	{
		return normalize_skill(current_morph_->get_trained_skill(skill_num));
	}
	return 0;
}

// * ������� ����� �� �� �����, � ��� ��������� ��� ���������� ������ ��� ������.
void CHAR_DATA::set_skill(const ESkill skill_num, int percent)
{
	if (skill_num < 0 || skill_num > MAX_SKILL_NUM)
	{
		log("SYSERROR: ���������� ����� ������ %d � set_skill.", skill_num);
		return;
	}

	CharSkillsType::iterator it = skills.find(skill_num);
	if (it != skills.end())
	{
		if (percent)
			it->second = percent;
		else
			skills.erase(it);
	}
	else if (percent)
		skills[skill_num] = percent;
}

void CHAR_DATA::set_skill(short remort)
{
int skill;
	for (auto it=skills.begin();it!=skills.end();it++)
	{
		skill = get_trained_skill((*it).first) + get_equipped_skill((*it).first);
		if (skill > 80 + remort*5)
			it->second = 80 + remort*5;
		
	}

}

void CHAR_DATA::set_morphed_skill(const ESkill skill_num, int percent)
{
	current_morph_->set_skill(skill_num, percent);
};

void CHAR_DATA::clear_skills()
{
	skills.clear();
}
// �������� ��� ������ �� �������� �� ������
void CHAR_DATA::crop_skills()
{
	int skill;
	for (auto it = skills.begin();it != skills.end();it++)
	{
		skill = get_trained_skill((*it).first) + get_equipped_skill((*it).first);
		if (skill > 80 + this->get_remort() * 5)
			it->second = 80 + this->get_remort() * 5;
	}
}


int CHAR_DATA::get_skills_count() const
{
	return static_cast<int>(skills.size());
}

int CHAR_DATA::get_obj_slot(int slot_num)
{
	if (slot_num >= 0 && slot_num < MAX_ADD_SLOTS)
	{
		return add_abils.obj_slot[slot_num];
	}
	return 0;
}

void CHAR_DATA::add_obj_slot(int slot_num, int count)
{
	if (slot_num >= 0 && slot_num < MAX_ADD_SLOTS)
	{
		add_abils.obj_slot[slot_num] += count;
	}
}

void CHAR_DATA::set_touching(CHAR_DATA *vict)
{
	touching_ = vict;
	check_fighting_list();
}

CHAR_DATA * CHAR_DATA::get_touching() const
{
	return touching_;
}

void CHAR_DATA::set_protecting(CHAR_DATA *vict)
{
	protecting_ = vict;
	check_fighting_list();
}

CHAR_DATA * CHAR_DATA::get_protecting() const
{
	return protecting_;
}

void CHAR_DATA::set_fighting(CHAR_DATA *vict)
{
	fighting_ = vict;
	check_fighting_list();
}

CHAR_DATA * CHAR_DATA::get_fighting() const
{
	return fighting_;
}

void CHAR_DATA::set_extra_attack(ExtraAttackEnumType Attack, CHAR_DATA *vict)
{
	extra_attack_.used_attack = Attack;
	extra_attack_.victim = vict;
	check_fighting_list();
}

ExtraAttackEnumType CHAR_DATA::get_extra_attack_mode() const
{
	return extra_attack_.used_attack;
}

CHAR_DATA * CHAR_DATA::get_extra_victim() const
{
	return extra_attack_.victim;
}

void CHAR_DATA::set_cast(int spellnum, int spell_subst, CHAR_DATA *tch, OBJ_DATA *tobj, ROOM_DATA *troom)
{
	cast_attack_.spellnum = spellnum;
	cast_attack_.spell_subst = spell_subst;
	cast_attack_.tch = tch;
	cast_attack_.tobj = tobj;
	cast_attack_.troom = troom;
	check_fighting_list();
}

int CHAR_DATA::get_cast_spell() const
{
	return cast_attack_.spellnum;
}

int CHAR_DATA::get_cast_subst() const
{
	return cast_attack_.spell_subst;
}

CHAR_DATA * CHAR_DATA::get_cast_char() const
{
	return cast_attack_.tch;
}

OBJ_DATA * CHAR_DATA::get_cast_obj() const
{
	return cast_attack_.tobj;
}

void CHAR_DATA::check_fighting_list()
{
	/*
	if (!in_fighting_list_)
	{
		in_fighting_list_ = true;
		fighting_list.push_back(this);
	}
	*/
}

void CHAR_DATA::clear_fighing_list()
{
	if (in_fighting_list_)
	{
		in_fighting_list_ = false;
		std::list<CHAR_DATA *>::iterator it =  std::find(fighting_list.begin(), fighting_list.end(), this);
		if (it != fighting_list.end())
		{
			fighting_list.erase(it);
		}
	}
}

bool IS_CHARMICE(const CHAR_DATA* ch)
{
	return IS_NPC(ch)
		&& (AFF_FLAGGED(ch, EAffectFlag::AFF_HELPER)
			|| AFF_FLAGGED(ch, EAffectFlag::AFF_CHARM));
}

bool MORT_CAN_SEE(const CHAR_DATA* sub, const CHAR_DATA* obj)
{
	return HERE(obj)
		&& INVIS_OK(sub, obj)
		&& (IS_LIGHT((obj)->in_room)
			|| AFF_FLAGGED((sub), EAffectFlag::AFF_INFRAVISION));
}

bool MAY_SEE(const CHAR_DATA* ch, const CHAR_DATA* sub, const CHAR_DATA* obj)
{
	return !(GET_INVIS_LEV(ch) > 30)
		&& !AFF_FLAGGED(sub, EAffectFlag::AFF_BLIND)
		&& (!IS_DARK(sub->in_room)
			|| AFF_FLAGGED(sub, EAffectFlag::AFF_INFRAVISION))
		&& (!AFF_FLAGGED(obj, EAffectFlag::AFF_INVISIBLE)
			|| AFF_FLAGGED(sub, EAffectFlag::AFF_DETECT_INVIS));
}

bool IS_HORSE(const CHAR_DATA* ch)
{
	return IS_NPC(ch)
		&& ch->has_master()
		&& AFF_FLAGGED(ch, EAffectFlag::AFF_HORSE);
}

bool IS_MORTIFIER(const CHAR_DATA* ch)
{
	return IS_NPC(ch)
		&& ch->has_master()
		&& MOB_FLAGGED(ch, MOB_CORPSE);
}

bool MAY_ATTACK(const CHAR_DATA* sub)
{
	return (!AFF_FLAGGED((sub), EAffectFlag::AFF_CHARM)
		&& !IS_HORSE((sub))
		&& !AFF_FLAGGED((sub), EAffectFlag::AFF_STOPFIGHT)
		&& !AFF_FLAGGED((sub), EAffectFlag::AFF_MAGICSTOPFIGHT)
		&& !AFF_FLAGGED((sub), EAffectFlag::AFF_HOLD)
		&& !AFF_FLAGGED((sub), EAffectFlag::AFF_SLEEP)
		&& !MOB_FLAGGED((sub), MOB_NOFIGHT)
		&& GET_WAIT(sub) <= 0
		&& !sub->get_fighting()
		&& GET_POS(sub) >= POS_RESTING);
}

bool AWAKE(const CHAR_DATA* ch)
{
	return GET_POS(ch) > POS_SLEEPING
		&& !AFF_FLAGGED(ch, EAffectFlag::AFF_SLEEP);
}

bool OK_GAIN_EXP(const CHAR_DATA* ch, const CHAR_DATA* victim)
{
	return !NAME_BAD(ch)
		&& (NAME_FINE(ch)
			|| !(GET_LEVEL(ch) == NAME_LEVEL))
		&& !ROOM_FLAGGED(ch->in_room, ROOM_ARENA)
		&& IS_NPC(victim)
		&& (GET_EXP(victim) > 0)
		&& (!IS_NPC(victim)
			|| !IS_NPC(ch)
			|| AFF_FLAGGED(ch, EAffectFlag::AFF_CHARM))
		&& !IS_HORSE(victim);
}

bool IS_MALE(const CHAR_DATA* ch)
{
	return GET_SEX(ch) == ESex::SEX_MALE;
}

bool IS_FEMALE(const CHAR_DATA* ch)
{
	return GET_SEX(ch) == ESex::SEX_FEMALE;
}

bool IS_NOSEXY(const CHAR_DATA* ch)
{
	return GET_SEX(ch) == ESex::SEX_NEUTRAL;
}

bool IS_POLY(const CHAR_DATA* ch)
{
	return GET_SEX(ch) == ESex::SEX_POLY;
}

int VPOSI_MOB(const CHAR_DATA *ch, const int stat_id, const int val)
{
	const int character_class = ch->get_class();
	return IS_NPC(ch)
		? VPOSI(val, 1, 100)
		: VPOSI(val, 1, class_stats_limit[character_class][stat_id]);
}

bool IMM_CAN_SEE(const CHAR_DATA* sub, const CHAR_DATA* obj)
{
	return MORT_CAN_SEE(sub, obj)
		|| (!IS_NPC(sub)
			&& PRF_FLAGGED(sub, PRF_HOLYLIGHT));
}

bool CAN_SEE(const CHAR_DATA* sub, const CHAR_DATA* obj)
{
	return SELF(sub, obj)
		|| ((GET_REAL_LEVEL(sub) >= (IS_NPC(obj) ? 0 : GET_INVIS_LEV(obj)))
			&& IMM_CAN_SEE(sub, obj));
}

// * ������ ����� ��� ����� �� �������� � ��� ������ �������������� �� ��������.
void change_fighting(CHAR_DATA* ch, int need_stop)
{
	for (const auto& k : character_list)
	{
		if (k->get_protecting() == ch)
		{
			k->set_protecting(0);
			CLR_AF_BATTLE(k, EAF_PROTECT);
		}

		if (k->get_touching() == ch)
		{
			k->set_touching(0);
			CLR_AF_BATTLE(k, EAF_PROTECT);
		}

		if (k->get_extra_victim() == ch)
		{
			k->set_extra_attack(EXTRA_ATTACK_UNUSED, 0);
		}

		if (k->get_cast_char() == ch)
		{
			k->set_cast(0, 0, 0, 0, 0);
		}

		if (k->get_fighting() == ch && IN_ROOM(k) != NOWHERE)
		{
			log("[Change fighting] Change victim");

			bool found = false;
			for (const auto j : world[ch->in_room]->people)
			{
				if (j->get_fighting() == k.get())
				{
					act("�� ����������� �������� �� $N3.", FALSE, k.get(), 0, j, TO_CHAR);
					act("$n ����������$u �� ���!", FALSE, k.get(), 0, j, TO_VICT);
					k->set_fighting(j);
					found = true;

					break;
				}
			}

			if (!found
				&& need_stop)
			{
				stop_fighting(k.get(), FALSE);
			}
		}
	}
}

size_t fighting_list_size()
{
	return fighting_list.size();
}

int CHAR_DATA::get_serial_num()
{
	return serial_num_;
}

void CHAR_DATA::set_serial_num(int num)
{
	serial_num_ = num;
}

bool CHAR_DATA::purged() const
{
	return purged_;
}

const std::string & CHAR_DATA::get_name_str() const
{
	if (IS_NPC(this))
	{
		return short_descr_;
	}
	return name_;
}

const std::string& CHAR_DATA::get_name() const
{
	return IS_NPC(this) ? get_npc_name() : get_pc_name();
}

void CHAR_DATA::set_name(const char *name)
{
	if (IS_NPC(this))
	{
		set_npc_name(name);
	}
	else
	{
		set_pc_name(name);
	}
}

void CHAR_DATA::set_pc_name(const char *name)
{
	if (name)
	{
		name_ = name;
	}
	else
	{
		name_.clear();
	}
}

void CHAR_DATA::set_npc_name(const char *name)
{
	if (name)
	{
		short_descr_ = name;
	}
	else
	{
		short_descr_.clear();
	}
}

const char* CHAR_DATA::get_pad(unsigned pad) const
{
	if (pad < player_data.PNames.size())
	{
		return player_data.PNames[pad];
	}
	return 0;
}

void CHAR_DATA::set_pad(unsigned pad, const char* s)
{
	if (pad >= player_data.PNames.size())
	{
		return;
	}

	int i;
	if (GET_PAD(this, pad))
	{
		 //if this is a player, or a non-prototyped non-player
		bool f = !IS_NPC(this) || (IS_NPC(this) && GET_MOB_RNUM(this) == -1);
		//prototype mob, field modified
		if (!f) f |= (i = GET_MOB_RNUM(this)) >= 0 && GET_PAD(this, pad) != GET_PAD(&mob_proto[i], pad);
		if (f) free(GET_PAD(this, pad));
	}
	GET_PAD(this, pad) = str_dup(s);
}

const char* CHAR_DATA::get_long_descr() const
{
	return player_data.long_descr;
}

void CHAR_DATA::set_long_descr(const char* s)
{
	int i;
	if (player_data.long_descr)
	{
		bool f = !IS_NPC(this) || (IS_NPC(this) && GET_MOB_RNUM(this) == -1); //if this is a player, or a non-prototyped non-player
		if (!f) f |= (i = GET_MOB_RNUM(this)) >= 0 && player_data.long_descr != mob_proto[i].player_data.long_descr; //prototype mob, field modified
		if (f) free(player_data.long_descr);
	}
	player_data.long_descr = str_dup(s);
}

const char* CHAR_DATA::get_description() const
{
	return player_data.description;
}

void CHAR_DATA::set_description(const char* s)
{
	int i;
	if (player_data.description)
	{
		bool f = !IS_NPC(this) || (IS_NPC(this) && GET_MOB_RNUM(this) == -1); //if this is a player, or a non-prototyped non-player
		if (!f) f |= (i = GET_MOB_RNUM(this)) >= 0 && player_data.description != mob_proto[i].player_data.description; //prototype mob, field modified
		if (f) free(player_data.description);
	}
	player_data.description = str_dup(s);
}

short CHAR_DATA::get_class() const
{
	return chclass_;
}

void CHAR_DATA::set_class(short chclass)
{
	if (chclass < 0 || chclass > NPC_CLASS_LAST)	// Range includes player classes and NPC classes (and does not consider gaps between them).
	{
		log("WARNING: chclass=%d (%s:%d %s)", chclass, __FILE__, __LINE__, __func__);
	}
	chclass_ = chclass;
}

short CHAR_DATA::get_level() const
{
	return level_;
}

void CHAR_DATA::set_level(short level)
{
	if (IS_NPC(this))
	{
		level_ = std::max(static_cast<short>(0), std::min(MAX_MOB_LEVEL, level));
	}
	else
	{
		level_ = std::max(static_cast<short>(0), std::min(LVL_IMPL, level));
	}
}

long CHAR_DATA::get_idnum() const
{
	return idnum_;
}

void CHAR_DATA::set_idnum(long idnum)
{
	idnum_ = idnum;
}

int CHAR_DATA::get_uid() const
{
	return uid_;
}

void CHAR_DATA::set_uid(int uid)
{
	uid_ = uid;
}

long CHAR_DATA::get_exp() const
{
	return exp_;
}

void CHAR_DATA::set_exp(long exp)
{
	if (exp < 0)
	{
		log("WARNING: exp=%ld name=[%s] (%s:%d %s)", exp, get_name().c_str(), __FILE__, __LINE__, __func__);
	}
	exp_ = MAX(0, exp);
	msdp_report(msdp::constants::EXPERIENCE);
}

short CHAR_DATA::get_remort() const
{
	return remorts_;
}

void CHAR_DATA::set_remort(short num)
{
	remorts_ = MAX(0, num);
}

time_t CHAR_DATA::get_last_logon() const
{
	return last_logon_;
}

void CHAR_DATA::set_last_logon(time_t num)
{
	last_logon_ = num;
}

time_t CHAR_DATA::get_last_exchange() const
{
	return last_exchange_;
}

void CHAR_DATA::set_last_exchange(time_t num)
{
	last_exchange_ = num;
}

ESex CHAR_DATA::get_sex() const
{
	return player_data.sex;
}

void CHAR_DATA::set_sex(const ESex sex)
{
	if (to_underlying(sex) >= 0
		&& to_underlying(sex) < NUM_SEXES)
	{
		player_data.sex = sex;
	}
}

ubyte CHAR_DATA::get_weight() const
{
	return player_data.weight;
}

void CHAR_DATA::set_weight(const ubyte v)
{
	player_data.weight = v;
}

ubyte CHAR_DATA::get_height() const
{
	return player_data.height;
}

void CHAR_DATA::set_height(const ubyte v)
{
	player_data.height = v;
}

ubyte CHAR_DATA::get_religion() const
{
	return player_data.Religion;
}

void CHAR_DATA::set_religion(const ubyte v)
{
	if (v < 2)
	{
		player_data.Religion = v;
	}
}

ubyte CHAR_DATA::get_kin() const
{
	return player_data.Kin;
}

void CHAR_DATA::set_kin(const ubyte v)
{
	if (v < NUM_KIN)
	{
		player_data.Kin = v;
	}
}

ubyte CHAR_DATA::get_race() const
{
	return player_data.Race;
}

void CHAR_DATA::set_race(const ubyte v)
{
	player_data.Race = v;
}

int CHAR_DATA::get_hit() const
{
	return points.hit;
}

void CHAR_DATA::set_hit(const int v)
{
	if (v>=-10)
		points.hit = v;
}

int CHAR_DATA::get_max_hit() const
{
	return points.max_hit;
}

void CHAR_DATA::set_max_hit(const int v)
{
	if (v >= 0)
	{
		points.max_hit = v;
	}
	msdp_report(msdp::constants::MAX_HIT);
}

sh_int CHAR_DATA::get_move() const
{
	return points.move;
}

void CHAR_DATA::set_move(const sh_int v)
{
	if (v >= 0)
		points.move = v;
}

sh_int CHAR_DATA::get_max_move() const
{
	return points.max_move;
}

void CHAR_DATA::set_max_move(const sh_int v)
{
	if (v >= 0)
	{
		points.max_move = v;
	}
	msdp_report(msdp::constants::MAX_MOVE);
}

long CHAR_DATA::get_ruble()
{
        return ruble;
}

void CHAR_DATA::set_ruble(int ruble)
{
        this->ruble = ruble;
}

long CHAR_DATA::get_gold() const
{
	return gold_;
}

long CHAR_DATA::get_bank() const
{
	return bank_gold_;
}

long CHAR_DATA::get_total_gold() const
{
	return get_gold() + get_bank();
}

/**
 * ���������� ����� �� �����, ��������� ������ ������������� �����.
 * \param need_log ����� � ����� - ���������� ��� ��� ��������� ����� (=true)
 * \param clan_tax - ��������� � ������� ����-����� ��� ��� (=false)
 */
void CHAR_DATA::add_gold(long num, bool need_log, bool clan_tax)
{
	if (num < 0)
	{
		log("SYSERROR: num=%ld (%s:%d %s)", num, __FILE__, __LINE__, __func__);
		return;
	}
	if (clan_tax)
	{
		num -= ClanSystem::do_gold_tax(this, num);
	}
	set_gold(get_gold() + num, need_log);
}

// * ��. add_gold()
void CHAR_DATA::add_bank(long num, bool need_log)
{
	if (num < 0)
	{
		log("SYSERROR: num=%ld (%s:%d %s)", num, __FILE__, __LINE__, __func__);
		return;
	}
	set_bank(get_bank() + num, need_log);
}

/**
 * ��� ����� �� �����, ������������� ����� ������ �������� ���� �
 * ������������ ������ �����.
 */
void CHAR_DATA::set_gold(long num, bool need_log)
{
	if (get_gold() == num)
	{
		// ����� � ������������ �� ��������������
		return;
	}
	num = MAX(0, MIN(MAX_MONEY_KEPT, num));

	if (need_log && !IS_NPC(this))
	{
		long change = num - get_gold();
		if (change > 0)
		{
			log("Gold: %s add %ld", get_name().c_str(), change);
		}
		else
		{
			log("Gold: %s remove %ld", get_name().c_str(), -change);
		}
		if (IN_ROOM(this) > 0)
		{
			MoneyDropStat::add(zone_table[world[IN_ROOM(this)]->zone].number, change);
		}
	}

	gold_ = num;
	msdp_report(msdp::constants::GOLD);
}

// * ��. set_gold()
void CHAR_DATA::set_bank(long num, bool need_log)
{
	if (get_bank() == num)
	{
		// ����� � ������������ �� ��������������
		return;
	}
	num = MAX(0, MIN(MAX_MONEY_KEPT, num));

	if (need_log && !IS_NPC(this))
	{
		long change = num - get_bank();
		if (change > 0)
		{
			log("Gold: %s add %ld", get_name().c_str(), change);
		}
		else
		{
			log("Gold: %s remove %ld", get_name().c_str(), -change);
		}
	}

	bank_gold_ = num;
	msdp_report(msdp::constants::GOLD);
}

/**
 * ������ ����������� �� ����� �����.
 * \param num - ������������� �����
 * \return - ���-�� ���, ������� �� ������� ����� � ��� (��������� �����)
 */
long CHAR_DATA::remove_gold(long num, bool need_log)
{
	if (num < 0)
	{
		log("SYSERROR: num=%ld (%s:%d %s)", num, __FILE__, __LINE__, __func__);
		return num;
	}
	if (num == 0)
	{
		return num;
	}

	long rest = 0;
	if (get_gold() >= num)
	{
		set_gold(get_gold() - num, need_log);
	}
	else
	{
		rest = num - get_gold();
		set_gold(0, need_log);
	}

	return rest;
}

// * ��. remove_gold()
long CHAR_DATA::remove_bank(long num, bool need_log)
{
	if (num < 0)
	{
		log("SYSERROR: num=%ld (%s:%d %s)", num, __FILE__, __LINE__, __func__);
		return num;
	}
	if (num == 0)
	{
		return num;
	}

	long rest = 0;
	if (get_bank() >= num)
	{
		set_bank(get_bank() - num, need_log);
	}
	else
	{
		rest = num - get_bank();
		set_bank(0, need_log);
	}

	return rest;
}

/**
 * ������� ������ ����� � ����� �, � ������ �������, � ���.
 * \return - ���-�� ���, ������� �� ������� ����� (��������� ����� � ����� � �� �����)
 */
long CHAR_DATA::remove_both_gold(long num, bool need_log)
{
	long rest = remove_bank(num, need_log);
	return remove_gold(rest, need_log);
}

// * ����� (������) ��� �������� � ������ � ������ ���� �� ���� ���.
int CHAR_DATA::calc_morale() const
{
	return GET_REAL_CHA(this) / 2 + GET_MORALE(this);
//	return cha_app[GET_REAL_CHA(this)].morale + GET_MORALE(this);
}
///////////////////////////////////////////////////////////////////////////////
int CHAR_DATA::get_str() const
{
	check_purged(this, "get_str");
	return current_morph_->GetStr();
}

int CHAR_DATA::get_inborn_str() const
{
	return str_;
}

void CHAR_DATA::set_str(int param)
{
	str_ = MAX(1, param);
}

void CHAR_DATA::inc_str(int param)
{
	str_ = MAX(1, str_+param);
}

int CHAR_DATA::get_str_add() const
{
	return str_add_;
}

void CHAR_DATA::set_str_add(int param)
{
	str_add_ = param;
}
///////////////////////////////////////////////////////////////////////////////
int CHAR_DATA::get_dex() const
{
	check_purged(this, "get_dex");
	return current_morph_->GetDex();
}

int CHAR_DATA::get_inborn_dex() const
{
	return dex_;
}

void CHAR_DATA::set_dex(int param)
{
	dex_ = MAX(1, param);
}

void CHAR_DATA::inc_dex(int param)
{
	dex_ = MAX(1, dex_+param);
}


int CHAR_DATA::get_dex_add() const
{
	return dex_add_;
}

void CHAR_DATA::set_dex_add(int param)
{
	dex_add_ = param;
}
///////////////////////////////////////////////////////////////////////////////
int CHAR_DATA::get_con() const
{
	check_purged(this, "get_con");
	return current_morph_->GetCon();
}

int CHAR_DATA::get_inborn_con() const
{
	return con_;
}

void CHAR_DATA::set_con(int param)
{
	con_ = MAX(1, param);
}
void CHAR_DATA::inc_con(int param)
{
	con_ = MAX(1, con_+param);
}

int CHAR_DATA::get_con_add() const
{
	return con_add_;
}

void CHAR_DATA::set_con_add(int param)
{
	con_add_ = param;
}
//////////////////////////////////////

int CHAR_DATA::get_int() const
{
	check_purged(this, "get_int");
	return current_morph_->GetIntel();
}

int CHAR_DATA::get_inborn_int() const
{
	return int_;
}

void CHAR_DATA::set_int(int param)
{
	int_ = MAX(1, param);
}

void CHAR_DATA::inc_int(int param)
{
	int_ = MAX(1, int_+param);
}

int CHAR_DATA::get_int_add() const
{
	return int_add_;
}

void CHAR_DATA::set_int_add(int param)
{
	int_add_ = param;
}
////////////////////////////////////////
int CHAR_DATA::get_wis() const
{
	check_purged(this, "get_wis");
	return current_morph_->GetWis();
}

int CHAR_DATA::get_inborn_wis() const
{
	return wis_;
}

void CHAR_DATA::set_wis(int param)
{
	wis_ = MAX(1, param);
}

void CHAR_DATA::set_who_mana(unsigned int param)
{
	player_specials->saved.who_mana = param;
}

void CHAR_DATA::set_who_last(time_t param)
{
	char_specials.who_last = param;
}

unsigned int CHAR_DATA::get_who_mana()
{
	return player_specials->saved.who_mana;
}

time_t CHAR_DATA::get_who_last()
{
	return char_specials.who_last;
}

void CHAR_DATA::inc_wis(int param)
{
	wis_ = MAX(1, wis_ + param);
}


int CHAR_DATA::get_wis_add() const
{
	return wis_add_;
}

void CHAR_DATA::set_wis_add(int param)
{
	wis_add_ = param;
}
///////////////////////////////////////////////////////////////////////////////
int CHAR_DATA::get_cha() const
{
	check_purged(this, "get_cha");
	return current_morph_->GetCha();
}

int CHAR_DATA::get_inborn_cha() const
{
	return cha_;
}

void CHAR_DATA::set_cha(int param)
{
	cha_ = MAX(1, param);
}
void CHAR_DATA::inc_cha(int param)
{
	cha_ = MAX(1, cha_+param);
}


int CHAR_DATA::get_cha_add() const
{
	return cha_add_;
}

void CHAR_DATA::set_cha_add(int param)
{
	cha_add_ = param;
}
///////////////////////////////////////////////////////////////////////////////

void CHAR_DATA::clear_add_affects()
{
	// Clear all affect, because recalc one
	memset(&add_abils, 0, sizeof(char_played_ability_data));
	set_str_add(0);
	set_dex_add(0);
	set_con_add(0);
	set_int_add(0);
	set_wis_add(0);
	set_cha_add(0);
}
///////////////////////////////////////////////////////////////////////////////
int CHAR_DATA::get_zone_group() const
{
	if (IS_NPC(this) && nr >= 0 && mob_index[nr].zone >= 0)
	{
		return MAX(1, zone_table[mob_index[nr].zone].group);
	}
	return 1;
}

//===================================
//Polud ����� � ��� ��� � ���� �������
//===================================

bool CHAR_DATA::know_morph(const std::string& morph_id) const
{
	return std::find(morphs_.begin(), morphs_.end(), morph_id) != morphs_.end();
}

void CHAR_DATA::add_morph(const std::string& morph_id)
{
	morphs_.push_back(morph_id);
};

void CHAR_DATA::clear_morphs()
{
	morphs_.clear();
};


const CHAR_DATA::morphs_list_t& CHAR_DATA::get_morphs()
{
	return morphs_;
};

std::string CHAR_DATA::get_title()
{
	if (!this->player_data.title)
	{
		return std::string();
	}
	std::string tmp = std::string(this->player_data.title);
	size_t pos = tmp.find('/');
	if (pos == std::string::npos)
	{
		return std::string();
	}
	tmp = tmp.substr(0, pos);
	pos = tmp.find(';');

	return pos == std::string::npos
		? tmp
		: tmp.substr(0, pos);
};

std::string CHAR_DATA::get_pretitle()
{
	if (!this->player_data.title) return std::string();
	std::string tmp = std::string(this->player_data.title);
	size_t pos = tmp.find('/');
	if (pos == std::string::npos)
	{
		return std::string();
	}
	tmp = tmp.substr(0, pos);
	pos = tmp.find(';');
	return pos == std::string::npos
		? std::string()
		: tmp.substr(pos + 1, tmp.length() - (pos + 1));
}

std::string CHAR_DATA::get_race_name()
{
	return PlayerRace::GetRaceNameByNum(GET_KIN(this),GET_RACE(this),GET_SEX(this));
}

std::string CHAR_DATA::get_morph_desc() const
{
	return current_morph_->GetMorphDesc();
};

std::string CHAR_DATA::get_morphed_name() const
{
	return current_morph_->GetMorphDesc() + " - " + this->get_name();
};

std::string CHAR_DATA::get_morphed_title() const
{
	return current_morph_->GetMorphTitle();
};

std::string CHAR_DATA::only_title_noclan()
{
	std::string result = this->get_name();
	std::string title = this->get_title();
	std::string pre_title = this->get_pretitle();

	if (!pre_title.empty())
		result = pre_title + " " + result;

	if (!title.empty() && this->get_level() >= MIN_TITLE_LEV)
		result = result + ", " + title;

	return result;
}

std::string CHAR_DATA::clan_for_title()
{
	std::string result = std::string();

	bool imm = IS_IMMORTAL(this) || PRF_FLAGGED(this, PRF_CODERINFO);

	if (CLAN(this) && !imm)
		result = result + "(" + GET_CLAN_STATUS(this) + ")";

	return result;
}

std::string CHAR_DATA::only_title()
{
	std::string result = this->clan_for_title();
	if (!result.empty())
		result = this->only_title_noclan() + " " + result;
	else
		result = this->only_title_noclan();

	return result;
}

std::string CHAR_DATA::noclan_title()
{
	std::string race = this->get_race_name();
	std::string result = this->only_title_noclan();

	if (result == this->get_name())
	{
		result = race + " " + result;
	}

	return result;
}

std::string CHAR_DATA::race_or_title()
{
	std::string result = this->clan_for_title();

	if (!result.empty())
		result = this->noclan_title() + " " + result;
	else
		result = this->noclan_title();

	return result;
}

size_t CHAR_DATA::get_morphs_count() const
{
	return morphs_.size();
};

std::string CHAR_DATA::get_cover_desc()
{
	return current_morph_->CoverDesc();
}

void CHAR_DATA::set_morph(MorphPtr morph)
{
	morph->SetChar(this);
	morph->InitSkills(this->get_skill(SKILL_MORPH));
	morph->InitAbils();
	this->current_morph_ = morph;
};

void CHAR_DATA::reset_morph()
{
	int value = this->get_trained_skill(SKILL_MORPH);
	send_to_char(str(boost::format(current_morph_->GetMessageToChar()) % "���������") + "\r\n", this);
	act(str(boost::format(current_morph_->GetMessageToRoom()) % "���������").c_str(), TRUE, this, 0, 0, TO_ROOM);
	this->current_morph_ = GetNormalMorphNew(this);
	this->set_morphed_skill(SKILL_MORPH, (MIN(MAX_EXP_PERCENT + GET_REMORT(this) * 5, value)));
//	REMOVE_BIT(AFF_FLAGS(this, AFF_MORPH), AFF_MORPH);
};

bool CHAR_DATA::is_morphed() const
{
	return current_morph_->Name() != "�������" || AFF_FLAGGED(this, EAffectFlag::AFF_MORPH);
};

void CHAR_DATA::set_normal_morph()
{
	current_morph_ = GetNormalMorphNew(this);
}

bool CHAR_DATA::isAffected(const EAffectFlag flag) const
{
	return current_morph_->isAffected(flag);
}

const IMorph::affects_list_t& CHAR_DATA::GetMorphAffects()
{
	return current_morph_->GetAffects();
}

//===================================
//-Polud
//===================================

bool CHAR_DATA::get_role(unsigned num) const
{
	bool result = false;
	if (num < role_.size())
	{
		result = role_.test(num);
	}
	else
	{
		log("SYSERROR: num=%u (%s:%d)", num, __FILE__, __LINE__);
	}
	return result;
}

void CHAR_DATA::set_role(unsigned num, bool flag)
{
	if (num < role_.size())
	{
		role_.set(num, flag);
	}
	else
	{
		log("SYSERROR: num=%u (%s:%d)", num, __FILE__, __LINE__);
	}
}

void CHAR_DATA::msdp_report(const std::string& name)
{
	if (nullptr != desc)
	{
		desc->msdp_report(name);
	}
}

void CHAR_DATA::add_follower(CHAR_DATA* ch)
{
	add_follower_silently(ch);

	if (!IS_HORSE(ch))
	{
		act("�� ������ ��������� �� $N4.", FALSE, ch, 0, this, TO_CHAR);
		act("$n �����$g ��������� �� ����.", TRUE, ch, 0, this, TO_VICT);
		act("$n �����$g ��������� �� $N4.", TRUE, ch, 0, this, TO_NOTVICT | TO_ARENA_LISTEN);
	}
}

CHAR_DATA::followers_list_t CHAR_DATA::get_followers_list() const
{
	CHAR_DATA::followers_list_t result;

	auto pos = followers;
	while (pos)
	{
		const auto follower = pos->follower;
		result.push_back(follower);
		pos = pos->next;
	}

	return result;
}

bool CHAR_DATA::low_charm() const
{
	for (const auto& aff : affected)
	{
		if (aff->type == SPELL_CHARM
			&& aff->duration <= 1)
		{
			return true;
		}
	}

	return false;
}

void CHAR_DATA::add_follower_silently(CHAR_DATA* ch)
{
	struct follow_type *k;

	if (ch->has_master())
	{
		log("SYSERR: add_follower_implementation(%s->%s) when master existing(%s)...",
			GET_NAME(ch), get_name().c_str(), GET_NAME(ch->get_master()));
		return;
	}

	if (ch == this)
	{
		return;
	}

	ch->set_master(this);

	CREATE(k, 1);

	k->follower = ch;
	k->next = followers;
	followers = k;
}

const CHAR_DATA::role_t& CHAR_DATA::get_role_bits() const
{
	return role_;
}

// ��������� ���������� ch ���� � ������ ��������� ����� � ���������� type
// ��� ��������� ��� ������ � ���� ������
void CHAR_DATA::add_attacker(CHAR_DATA *ch, unsigned type, int num)
{
	if (!IS_NPC(this) || IS_NPC(ch) || !get_role(MOB_ROLE_BOSS))
	{
		return;
	}

	int uid = ch->get_uid();
	if (IS_CHARMICE(ch) && ch->has_master())
	{
		uid = ch->get_master()->get_uid();
	}

	auto i = attackers_.find(uid);
	if (i != attackers_.end())
	{
		switch(type)
		{
		case ATTACKER_DAMAGE:
			i->second.damage += num;
			break;
		case ATTACKER_ROUNDS:
			i->second.rounds += num;
			break;
		}
	}
	else
	{
		attacker_node tmp_node;
		switch(type)
		{
		case ATTACKER_DAMAGE:
			tmp_node.damage = num;
			break;
		case ATTACKER_ROUNDS:
			tmp_node.rounds = num;
			break;
		}
		attackers_.insert(std::make_pair(uid, tmp_node));
	}
}

// ���������� �������������� �������� �� ����� type ���������� ch ����
// �� ������ ��������� ������� �����
int CHAR_DATA::get_attacker(CHAR_DATA *ch, unsigned type) const
{
	if (!IS_NPC(this) || IS_NPC(ch) || !get_role(MOB_ROLE_BOSS))
	{
		return -1;
	}
	auto i = attackers_.find(ch->get_uid());
	if (i != attackers_.end())
	{
		switch(type)
		{
		case ATTACKER_DAMAGE:
			return i->second.damage;
		case ATTACKER_ROUNDS:
			return i->second.rounds;
		}
	}
	return 0;
}

// ����� � ������ ��������� ��������� ������������ ����, ������� ��� ����
// ��������� � ������ ������ � ���� �� ������� � ������ � ������
std::pair<int /* uid */, int /* rounds */> CHAR_DATA::get_max_damager_in_room() const
{
	std::pair<int, int> damager (-1, 0);

	if (!IS_NPC(this) || !get_role(MOB_ROLE_BOSS))
	{
		return damager;
	}

	int max_dmg = 0;
	for (const auto i : world[this->in_room]->people)
	{
		if (!IS_NPC(i) && i->desc)
		{
			auto it = attackers_.find(i->get_uid());
			if (it != attackers_.end())
			{
				if (it->second.damage > max_dmg)
				{
					max_dmg = it->second.damage;
					damager.first = it->first;
					damager.second = it->second.rounds;
				}
			}
		}
	}

	return damager;
}

// ���������� ����� ��� ��� �� ���������� MOB_RESTORE_TIMER ������
void CHAR_DATA::restore_mob()
{
	restore_timer_ = 0;
	attackers_.clear();

	GET_HIT(this) = GET_REAL_MAX_HIT(this);
	GET_MOVE(this) = GET_REAL_MAX_MOVE(this);
	update_pos(this);

	for (int i = 0; i < MAX_SPELLS; ++i)
	{
		GET_SPELL_MEM(this, i) = GET_SPELL_MEM(&mob_proto[GET_MOB_RNUM(this)], i);
	}
	GET_CASTER(this) = GET_CASTER(&mob_proto[GET_MOB_RNUM(this)]);
}

void CHAR_DATA::report_loop_error(const CHAR_DATA::ptr_t master) const
{
	std::stringstream ss;
	ss << "���������� ������ ������: ������� ������� ���� � ������� ��������������.\n������� ������� �������: ";
	master->print_leaders_chain(ss);
	ss << "\n������� ������� ��������� [" << master->get_name() << "] ������� ��������� [" << get_name() << "]";
	mudlog(ss.str().c_str(), DEF, -1, ERRLOG, true);

	std::stringstream additional_info;
	additional_info << "������������� �����: name=[" << master->get_name() << "]"
		<< "; ����� ���������: " << master << "; ������� �����: ";
	if (master->has_master())
	{
		additional_info << "name=[" << master->get_master()->get_name() << "]"
			<< "; ����� ���������: " << master->get_master() << "";
	}
	else
	{
		additional_info << "<�����������>";
	}
	additional_info << "\n�������������: name=[" << get_name() << "]"
		<< "; ����� ���������: " << this << "; ������� �����: ";
	if (has_master())
	{
		additional_info << "name=[" << get_master()->get_name() << "]"
			<< "; ����� ���������: " << get_master() << "";
	}
	else
	{
		additional_info << "<�����������>";
	}
	mudlog(additional_info.str().c_str(), DEF, -1, ERRLOG, true);

	ss << "\n������� ���� ����� ���������� � ERRLOG.";
	debug::backtrace(runtime_config.logs(ERRLOG).handle());
	mudlog(ss.str().c_str(), DEF, LVL_IMPL, ERRLOG, false);
}

void CHAR_DATA::print_leaders_chain(std::ostream& ss) const
{
	if (!has_master())
	{
		ss << "<�����>";
		return;
	}

	bool first = true;
	for (auto master = get_master(); master; master = master->get_master())
	{
		ss << (first ? "" : " -> ") << "[" << master->get_name() << "]";
		first = false;
	}
}

void CHAR_DATA::set_master(CHAR_DATA::ptr_t master)
{
	if (makes_loop(master))
	{
		report_loop_error(master);
		return;
	}
	m_master = master;
}

bool CHAR_DATA::makes_loop(CHAR_DATA::ptr_t master) const
{
	while (master)
	{
		if (master == this)
		{
			return true;
		}
		master = master->get_master();
	}

	return false;
}

// ��������� � �������� ������� �� ������ �����,
// ������� ��������� ��� ��� � �� ����� ��� ���-�� ���
// (�.�. ����� �� ������� ������ ��������)
void CHAR_DATA::inc_restore_timer(int num)
{
	if (get_role(MOB_ROLE_BOSS) && !attackers_.empty() && !get_fighting())
	{
		restore_timer_ += num;
		if (restore_timer_ > num)
		{
			restore_mob();
		}
	}
}

obj_sets::activ_sum& CHAR_DATA::obj_bonus()
{
	return obj_bonus_;
}

player_special_data::player_special_data() :
	poofin(nullptr),
	poofout(nullptr),
	aliases(nullptr),
	may_rent(0),
	agressor(0),
	agro_time(0),
	rskill(0),
	portals(0),
	logs(nullptr),
	Exchange_filter(nullptr),
	Karma(nullptr),
	logons(nullptr),
	clanStatus(nullptr)
{
}

player_special_data_saved::player_special_data_saved() :
	wimp_level(0),
	invis_level(0),
	load_room(0),
	bad_pws(0),
	DrunkState(0),
	olc_zone(0),
	NameGod(0),
	NameIDGod(0),
	GodsLike(0),
	stringLength(0),
	stringWidth(0),
	Rip_arena(0),
	Rip_mob(0),
	Rip_pk(0),
	Rip_dt(0),
	Rip_other(0),
	Win_arena(0),
	Rip_mob_this(0),
	Rip_pk_this(0),
	Rip_dt_this(0),
	Rip_other_this(0),
	Exp_arena(0),
	Exp_mob(0),
	Exp_pk(0),
	Exp_dt(0),
	Exp_other(0),
	Exp_mob_this(0),
	Exp_pk_this(0),
	Exp_dt_this(0),
	Exp_other_this(0),
	ntfyExchangePrice(0),
	HiredCost(0),
	who_mana(0)
{
	memset(EMail, 0, sizeof(EMail));
	memset(LastIP, 0, sizeof(LastIP));
}

// dummy spec area for mobs
player_special_data::shared_ptr player_special_data::s_for_mobiles = std::make_shared<player_special_data>();
																						
// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
