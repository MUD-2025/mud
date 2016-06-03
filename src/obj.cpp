// $RCSfile$     $Date$     $Revision$
// Copyright (c) 2009 Krodo
// Part of Bylins http://www.mud.ru

#include "obj.hpp"

#include "parse.hpp"
#include "handler.h"
#include "dg_scripts.h"
#include "screen.h"
#include "celebrates.hpp"
#include "pk.h"
#include "cache.hpp"
#include "char.hpp"
#include "depot.hpp"
#include "constants.h"
#include "db.h"
#include "utils.h"
#include "conf.h"

#include <boost/algorithm/string.hpp>

#include <cmath>
#include <sstream>

extern void get_from_container(CHAR_DATA * ch, OBJ_DATA * cont, char *arg, int mode, int amount, bool autoloot);
extern int Crash_write_timer(int index);	// to avoid inclusion of "objsave.h"

id_to_set_info_map OBJ_DATA::set_table;

namespace
{

// ������ ������ ����� ����� ��� ������������ �������� ��������
typedef std::vector<OBJ_DATA *> PurgedObjList;
PurgedObjList purged_obj_list;

} // namespace

OBJ_DATA::OBJ_DATA():
	CObjectPrototype(),
	m_uid(0),
	m_in_room(0),
	m_room_was_in(0),
	m_maker(DEFAULT_MAKER),
	m_owner(DEFAULT_OWNER),
	m_zone(0),
	m_parent(DEFAULT_PARENT),
	m_is_rename(false),
	m_carried_by(nullptr),
	m_worn_by(nullptr),
	m_worn_on(0),
	m_in_obj(nullptr),
	m_contains(nullptr),
	m_next_content(nullptr),
	m_next(nullptr),
	m_craft_timer(0),
	m_id(0),
	m_serial_number(0),
	m_purged(false),
	m_activator(false, 0)
{
	this->zero_init();
	caching::obj_cache.add(this);
}

OBJ_DATA::OBJ_DATA(const CObjectPrototype& other):
	CObjectPrototype(other),
	m_uid(0),
	m_in_room(0),
	m_room_was_in(0),
	m_maker(DEFAULT_MAKER),
	m_owner(DEFAULT_OWNER),
	m_zone(0),
	m_parent(DEFAULT_PARENT),
	m_is_rename(false),
	m_carried_by(nullptr),
	m_worn_by(nullptr),
	m_worn_on(0),
	m_in_obj(nullptr),
	m_contains(nullptr),
	m_next_content(nullptr),
	m_next(nullptr),
	m_craft_timer(0),
	m_id(0),
	m_serial_number(0),
	m_purged(false),
	m_activator(false, 0)
{
	caching::obj_cache.add(this);
}

OBJ_DATA::OBJ_DATA(const OBJ_DATA& other)
{
	*this = other;
	caching::obj_cache.add(this);
}

OBJ_DATA::~OBJ_DATA()
{
	if (!m_purged)
	{
		this->purge(true);
	}
}

// * ��. Character::zero_init()
void OBJ_DATA::zero_init()
{
	CObjectPrototype::zero_init();
	set_weight(0);
	m_uid = 0;
	m_in_room = NOWHERE;
	m_carried_by = nullptr;
	m_worn_by = nullptr;
	m_worn_on = NOWHERE;
	m_in_obj = nullptr;
	m_contains = nullptr;
	m_id = 0;
	m_script = nullptr;
	m_next_content = nullptr;
	m_next = nullptr;
	m_room_was_in = NOWHERE;
	m_serial_number = 0;
	m_purged = false;
	m_activator.first = false;
	m_activator.second = 0;

	m_custom_label = nullptr;
}

// * ��. Character::purge()
void OBJ_DATA::purge(bool destructor)
{
	if (m_purged)
	{
		log("SYSERROR: double purge (%s:%d)", __FILE__, __LINE__);
		return;
	}

	caching::obj_cache.remove(this);
	//��. ����������� � ��������� BloodyInfo �� pk.cpp
	bloody::remove_obj(this);
	//weak_ptr ��� �� ��� ������ � ����
	Celebrates::remove_from_obj_lists(this->get_uid());

	if (!destructor)
	{
		// �������� ���
		this->zero_init();
		// ����������� ������������ �� ������������ ����
		m_purged = true;
		// ���������� � ������ ��������� ������ ����������
		purged_obj_list.push_back(this);
	}
}

bool OBJ_DATA::purged() const
{
	return m_purged;
}

int OBJ_DATA::get_serial_num()
{
	return m_serial_number;
}

void OBJ_DATA::set_serial_num(int num)
{
	m_serial_number = num;
}

const std::string OBJ_DATA::activate_obj(const activation& __act)
{
	if (get_rnum() >= 0)
	{
		set_affect_flags(__act.get_affects());
		for (int i = 0; i < MAX_OBJ_AFFECT; i++)
		{
			set_affected(i, __act.get_affected_i(i));
		}

		int weight = __act.get_weight();
		if (weight > 0)
		{
			set_weight(weight);
		}

		if (get_type() == ITEM_WEAPON)
		{
			int nsides, ndices;
			__act.get_dices(ndices, nsides);
			// ���� ����� �������� �� ��, ��������������� �� ��� ���������.
			if (ndices > 0 && nsides > 0)
			{
				set_val(1, ndices);
				set_val(2, nsides);
			}
		}

		// ���������� ������.
		if (__act.has_skills())
		{
			// � ���� �������� � �������� skills ��������� �� ���� � ��� ��
			// ������. � � ����������. ������� ��� ���� ��������� �����,
			// ���� ��� ������� "������������" ����� ��� ����� �������.
			// ������, ������������� � ����, �������� ������ ������ ��������.
			skills_t skills;
			__act.get_skills(skills);
			set_skills(skills);
		}

		return __act.get_actmsg() + "\n" + __act.get_room_actmsg();
	}
	else
	{
		return "\n";
	}
}

const std::string OBJ_DATA::deactivate_obj(const activation& __act)
{
	if (get_rnum() >= 0)
	{
		set_affect_flags(obj_proto[get_rnum()]->get_affect_flags());
		for (int i = 0; i < MAX_OBJ_AFFECT; i++)
		{
			set_affected(i, obj_proto[get_rnum()]->get_affected(i));
		}

		set_weight(obj_proto[get_rnum()]->get_weight());

		if (get_type() == ITEM_WEAPON)
		{
			set_val(1, obj_proto[get_rnum()]->get_val(1));
			set_val(2, obj_proto[get_rnum()]->get_val(2));
		}

		// ������������ ������.
		if (__act.has_skills())
		{
			// ��� ��������� �� ��������� ����� ������ � ��������. ���
			// ����� ����� �������.
			set_skills(obj_proto[get_rnum()]->get_skills());
		}

		return __act.get_deactmsg() + "\n" + __act.get_room_deactmsg();
	}
	else
	{
		return "\n";
	}
}

void OBJ_DATA::set_script(SCRIPT_DATA* _)
{
	m_script.reset(_);
}

void CObjectPrototype::set_skill(int skill_num, int percent)
{
	if (!m_skills.empty())
	{
		const auto skill = m_skills.find(static_cast<ESkill>(skill_num));
		if (skill == m_skills.end())
		{
			if (percent != 0)
			{
				m_skills.insert(std::make_pair(static_cast<ESkill>(skill_num), percent));
			}
		}
		else
		{
			if (percent != 0)
			{
				skill->second = percent;
			}
			else
			{
				m_skills.erase(skill);
			}
		}
	}
	else
	{
		if (percent != 0)
		{
			m_skills.clear();
			m_skills.insert(std::make_pair(static_cast<ESkill>(skill_num), percent));
		}
	}
}

void CObjectPrototype::clear_all_affected()
{
	for (size_t i = 0; i < MAX_OBJ_AFFECT; i++)
	{
		if (m_affected[i].location != APPLY_NONE)
		{
			m_affected[i].location = APPLY_NONE;
		}
	}
}

void CObjectPrototype::zero_init()
{
	m_type = ITEM_UNDEFINED;
	m_aliases.clear();
	m_description.clear();
	m_short_description.clear();
	m_action_description.clear();
	m_ex_description.reset();
	m_proto_script.clear();
	m_max_in_world = 0;
	m_skills.clear();
	m_timer = 0;
	m_minimum_remorts = -1;
	m_cost = 0;
	m_rent_on = 0;
	m_rent_off = 0;
	for (int i = 0; i < 6; i++)
	{
		m_pnames[i].clear();
	}
	m_ilevel = 0;
}

int CObjectPrototype::get_skill(int skill_num) const
{
	const auto skill = m_skills.find(static_cast<ESkill>(skill_num));
	if (skill != m_skills.end())
	{
		return skill->second;
	}

	return 0;
}

// * @warning ��������������, ��� __out_skills.empty() == true.
void CObjectPrototype::get_skills(skills_t& out_skills) const
{
	if (!m_skills.empty())
	{
		out_skills.insert(m_skills.begin(), m_skills.end());
	}
}

bool CObjectPrototype::has_skills() const
{
	return !m_skills.empty();
}

void CObjectPrototype::set_timer(int timer)
{
	m_timer = MAX(0, timer);	
}

int CObjectPrototype::get_timer() const
{
	return m_timer;
}

 extern float count_remort_requred(const CObjectPrototype *obj);
 extern float count_unlimited_timer(const CObjectPrototype *obj);

/**
* �������� �������� ������ (��� ������ ����������� ����� ������� �� ����).
* ������ ������� ����� ������ ��������� ������ �� ���������� �������.
* \param time �� ������� 1.
*/
void OBJ_DATA::dec_timer(int time, bool ignore_utimer)
{
	if (!m_timed_spell.empty())
	{
		m_timed_spell.dec_timer(this, time);
	}

	if (!ignore_utimer && check_unlimited_timer(this))
	{
		return;
	}

	if (time > 0)
	{
		set_timer(get_timer() - time);
	}
}

float CObjectPrototype::show_mort_req() 
{
	return count_remort_requred(this);
}

float CObjectPrototype::show_koef_obj()
{
	return count_unlimited_timer(this);
}

unsigned CObjectPrototype::get_ilevel() const
{
	return m_ilevel;
}

void CObjectPrototype::set_ilevel(unsigned ilvl)
{
	m_ilevel = ilvl;
}

int CObjectPrototype::get_manual_mort_req() const
{
	if (get_minimum_remorts() >= 0)
	{
		return get_minimum_remorts();
	}
	else if (m_ilevel > 30)
	{
		return 9;
	}

	return 0;
}

void CObjectPrototype::set_cost(int x)
{
	if (x >= 0)
	{
		m_cost = x;
	}
}

void CObjectPrototype::set_rent_off(int x)
{
	if (x >= 0)
	{
		m_rent_off = x;
	}
}

void CObjectPrototype::set_rent_on(int x)
{
	if (x >= 0)
	{
		m_rent_on = x;
	}
}

void OBJ_DATA::set_activator(bool flag, int num)
{
	m_activator.first = flag;
	m_activator.second = num;
}

std::pair<bool, int> OBJ_DATA::get_activator() const
{
	return m_activator;
}

void OBJ_DATA::add_timed_spell(const int spell, const int time)
{
	if (spell < 1 || spell >= SPELLS_COUNT)
	{
		log("SYSERROR: func: %s, spell = %d, time = %d", __func__, spell, time);
		return;
	}
	m_timed_spell.add(this, spell, time);
}

void OBJ_DATA::del_timed_spell(const int spell, const bool message)
{
	m_timed_spell.del(this, spell, message);
}

void CObjectPrototype::set_ex_description(const char* keyword, const char* description)
{
	std::shared_ptr<EXTRA_DESCR_DATA> d(new EXTRA_DESCR_DATA());
	d->keyword = strdup(keyword);
	d->description = strdup(description);
	m_ex_description = d;
}

////////////////////////////////////////////////////////////////////////////////

namespace
{

const float SQRT_MOD = 1.7095f;
const int AFF_SHIELD_MOD = 30;
const int AFF_BLINK_MOD = 10;

} // namespace

////////////////////////////////////////////////////////////////////////////////

namespace ObjSystem
{

float count_affect_weight(const CObjectPrototype* /*obj*/, int num, int mod)
{
	float weight = 0;

	switch(num)
	{
	case APPLY_STR:
		weight = mod * 5.0;
		break;
	case APPLY_DEX:
		weight = mod * 10.0;
		break;
	case APPLY_INT:
		weight = mod * 10.0;
		break;
	case APPLY_WIS:
		weight = mod * 10.0;
		break;
	case APPLY_CON:
		weight = mod * 10.0;
		break;
	case APPLY_CHA:
		weight = mod * 10.0;
		break;
	case APPLY_HIT:
		weight = mod * 0.2;
		break;
	case APPLY_AC:
		weight = mod * -1.0;
		break;
	case APPLY_HITROLL:
		weight = mod * 3.3;
		break;
	case APPLY_DAMROLL:
		weight = mod * 3.3;
		break;
	case APPLY_SAVING_WILL:
		weight = mod * -1.0;
		break;
	case APPLY_SAVING_CRITICAL:
		weight = mod * -1.0;
		break;
	case APPLY_SAVING_STABILITY:
		weight = mod * -1.0;
		break;
	case APPLY_SAVING_REFLEX:
		weight = mod * -1.0;
		break;
	case APPLY_CAST_SUCCESS:
		weight = mod * 1.0;
		break;
	case APPLY_MORALE:
		weight = mod * 2.0;
		break;
	case APPLY_INITIATIVE:
		weight = mod * 1.0;
		break;
	case APPLY_ABSORBE:
		weight = mod * 1.0;
		break;
	}

	return weight;
}

bool is_armor_type(const CObjectPrototype *obj)
{
	switch (GET_OBJ_TYPE(obj))
	{
	case OBJ_DATA::ITEM_ARMOR:
	case OBJ_DATA::ITEM_ARMOR_LIGHT:
	case OBJ_DATA::ITEM_ARMOR_MEDIAN:
	case OBJ_DATA::ITEM_ARMOR_HEAVY:
		return true;

	default:
		return false;
	}
}

// * ��. CharacterSystem::release_purged_list()
void release_purged_list()
{
	for (PurgedObjList::iterator i = purged_obj_list.begin();
		i != purged_obj_list.end(); ++i)
	{
		delete *i;
	}
	purged_obj_list.clear();
}

bool is_mob_item(const CObjectPrototype *obj)
{
	if (IS_OBJ_NO(obj, ENoFlag::ITEM_NO_MALE)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_FEMALE)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_CHARMICE))
	{
		return true;
	}
	if (IS_OBJ_NO(obj, ENoFlag::ITEM_NO_MONO)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_POLY)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_CHARMICE))
	{
		return true;
	}
	if (IS_OBJ_NO(obj, ENoFlag::ITEM_NO_RUSICHI)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_CHARMICE))
	{
		return true;
	}
	if (IS_OBJ_NO(obj, ENoFlag::ITEM_NO_CLERIC)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_THIEF)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_WARRIOR)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_ASSASINE)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_GUARD)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_PALADINE)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_RANGER)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_SMITH)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_MERCHANT)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_DRUID)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_BATTLEMAGE)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_CHARMMAGE)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_DEFENDERMAGE)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_NECROMANCER)
		&& IS_OBJ_NO(obj, ENoFlag::ITEM_NO_CHARMICE))
	{
		return true;
	}
	if (IS_OBJ_NO(obj, ENoFlag::ITEM_NO_CHARMICE))
	{
		return true;
	}
	if (IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_MALE)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_FEMALE)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_CHARMICE))
	{
		return true;
	}
	if (IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_MONO)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_POLY)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_CHARMICE))
	{
		return true;
	}
	if (IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_RUSICHI)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_CHARMICE))
	{
		return true;
	}
	if (IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_CLERIC)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_THIEF)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_WARRIOR)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_ASSASINE)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_GUARD)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_PALADINE)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_RANGER)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_SMITH)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_MERCHANT)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_DRUID)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_BATTLEMAGE)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_CHARMMAGE)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_DEFENDERMAGE)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_NECROMANCER)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_CHARMICE))
	{
		return true;
	}
	if (IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_SEVERANE)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_POLANE)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_KRIVICHI)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_VATICHI)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_VELANE)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_DREVLANE)
		&& IS_OBJ_ANTI(obj, EAntiFlag::ITEM_AN_CHARMICE))
	{
		return true;
	}

	return false;
}

void init_ilvl(CObjectPrototype *obj)
{
	if (is_mob_item(obj)
		|| obj->get_extra_flag(EExtraFlag::ITEM_SETSTUFF)
		|| obj->get_manual_mort_req() >= 0)
	{
		obj->set_ilevel(0);
		return;
	}

	float total_weight = 0.0;

	// ������� APPLY_x
	for (int k = 0; k < MAX_OBJ_AFFECT; k++)
	{
		if (obj->get_affected(k).location == 0) continue;

		// ������, ���� ���� ������ �������� � ���������� �����
		for (int kk = 0; kk < MAX_OBJ_AFFECT; kk++)
		{
			if (obj->get_affected(k).location == obj->get_affected(kk).location
				&& k != kk)
			{
				log("SYSERROR: double affect=%d, obj_vnum=%d",
					obj->get_affected(k).location, GET_OBJ_VNUM(obj));
				obj->set_ilevel(1000000);
				return;
			}
		}
		//���� ������ �������������. ������ ������ �� �������
		if (obj->get_affected(k).modifier < 0)
		{
			continue;
		}
		float weight = count_affect_weight(obj, obj->get_affected(k).location, obj->get_affected(k).modifier);
		total_weight += pow(weight, SQRT_MOD);
	}
	// ������� AFF_x ����� weapon_affect
	for (const auto& m : weapon_affect)
	{
		if (IS_OBJ_AFF(obj, m.aff_pos))
		{
			if (static_cast<EAffectFlag>(m.aff_bitvector) == EAffectFlag::AFF_AIRSHIELD)
			{
				total_weight += pow(AFF_SHIELD_MOD, SQRT_MOD);
			}
			else if (static_cast<EAffectFlag>(m.aff_bitvector) == EAffectFlag::AFF_FIRESHIELD)
			{
				total_weight += pow(AFF_SHIELD_MOD, SQRT_MOD);
			}
			else if (static_cast<EAffectFlag>(m.aff_bitvector) == EAffectFlag::AFF_ICESHIELD)
			{
				total_weight += pow(AFF_SHIELD_MOD, SQRT_MOD);
			}
			else if (static_cast<EAffectFlag>(m.aff_bitvector) == EAffectFlag::AFF_BLINK)
			{
				total_weight += pow(AFF_BLINK_MOD, SQRT_MOD);
			}
		}
	}

	obj->set_ilevel(ceil(pow(total_weight, 1/SQRT_MOD)));
}

void init_item_levels()
{
	for (const auto i : obj_proto)
	{
		init_ilvl(i);
	}
}

} // namespace ObjSystem

namespace system_obj
{

/// ������� ��� ��� � ������
const int PURSE_VNUM = 1924;
int PURSE_RNUM = -1;
/// ������������ ���������
const int PERS_CHEST_VNUM = 331;
int PERS_CHEST_RNUM = -1;

/// ��� ������ ����� ����� ����� ���
void init()
{
	PURSE_RNUM = real_object(PURSE_VNUM);
	PERS_CHEST_RNUM = real_object(PERS_CHEST_VNUM);
}

OBJ_DATA* create_purse(CHAR_DATA *ch, int/* gold*/)
{
	OBJ_DATA *obj = read_object(PURSE_RNUM, REAL);
	if (!obj)
	{
		return obj;
	}

	obj->set_aliases("����� �������");
	obj->set_short_description("����� �������");
	obj->set_description("���-�� ���������� ����� ������� ����� �����.");
	obj->set_PName(0, "����� �������");
	obj->set_PName(1, "������ ��������");
	obj->set_PName(2, "������ ��������");
	obj->set_PName(3, "����� �������");
	obj->set_PName(4, "����� ���������");
	obj->set_PName(5, "����� ��������");

	char buf_[MAX_INPUT_LENGTH];
	snprintf(buf_, sizeof(buf_),
		"--------------------------------------------------\r\n"
		"��������: %s\r\n"
		"� ������ ������ ������� ������� �� ��������������.\r\n"
		"--------------------------------------------------\r\n"
		, ch->get_name().c_str());
	obj->set_ex_description(obj->get_PName(0).c_str(), buf_);

	obj->set_type(OBJ_DATA::ITEM_CONTAINER);
	obj->set_wear_flags(to_underlying(EWearFlag::ITEM_WEAR_TAKE));
	obj->set_val(0, 0);
	// CLOSEABLE + CLOSED
	obj->set_val(1,  5);
	obj->set_val(2, -1);
	obj->set_val(3, ch->get_uid());

	obj->set_rent_off(0);
	obj->set_rent_on(0);
	// ����� ������� ����� �� �������
	obj->set_cost(2);
	obj->set_extra_flag(EExtraFlag::ITEM_NODONATE);
	obj->set_extra_flag(EExtraFlag::ITEM_NOSELL);

	return obj;
}

bool is_purse(OBJ_DATA *obj)
{
	return obj->get_rnum() == PURSE_RNUM;
}

/// ���������� � ������ ������� ��� ������� ������� ��� ��� ������ ��������
void process_open_purse(CHAR_DATA *ch, OBJ_DATA *obj)
{
	auto value = obj->get_val(1);
	REMOVE_BIT(value, CONT_CLOSED);
	obj->set_val(1, value);

	char buf_[MAX_INPUT_LENGTH];
	snprintf(buf_, sizeof(buf_), "all");
	get_from_container(ch, obj, buf_, FIND_OBJ_INV, 1, false);
	act("$o ��������$U � ����� �����...", FALSE, ch, obj, 0, TO_CHAR);
	extract_obj(obj);
}

} // namespace system_obj

int ObjVal::get(const EValueKey key) const
{
	auto i = m_values.find(key);
	if (i != m_values.end())
	{
		return i->second;
	}
	return -1;
}

void ObjVal::set(const EValueKey key, int val)
{
	if (val >= 0)
	{
		m_values[key] = val;
	}
	else
	{
		auto i = m_values.find(key);
		if (i != m_values.end())
		{
			m_values.erase(i);
		}
	}
}

void ObjVal::inc(const ObjVal::EValueKey key, int val)
{
	auto i = m_values.find(key);
	if (i == m_values.end() || val == 0) return;

	if (val < 0 && i->second + val <= 0)
	{
		i->second = 0;
		return;
	}
	else if (val >= std::numeric_limits<int>::max() - i->second)
	{
		i->second = std::numeric_limits<int>::max();
		return;
	}

	i->second += val;
}

std::string ObjVal::print_to_file() const
{
	if (m_values.empty()) return "";

	std::stringstream out;
	out << "Vals:\n";

	for(auto i = m_values.begin(), iend = m_values.end(); i != iend; ++i)
	{
		std::string key_str = TextId::to_str(TextId::OBJ_VALS, to_underlying(i->first));
		if (!key_str.empty())
		{
			out << key_str << " " << i->second << "\n";
		}
	}
	out << "~\n";

	return out.str();
}

bool ObjVal::init_from_file(const char *str)
{
	m_values.clear();
	std::stringstream text(str);
	std::string key_str;
	bool result = true;
	int val = -1;

	while (text >> key_str >> val)
	{
		const int key = TextId::to_num(TextId::OBJ_VALS, key_str);
		if (key >= 0 && val >= 0)
		{
			m_values.emplace(static_cast<EValueKey>(key), val);
			key_str.clear();
			val = -1;
		}
		else
		{
			err_log("invalid key=%d or val=%d (%s %s:%d)",
				key, val, __func__, __FILE__, __LINE__);
		}
	}

	return result;
}

std::string ObjVal::print_to_zone() const
{
	if (m_values.empty())
	{
		return "";
	}

	std::stringstream out;

	for(auto i = m_values.begin(), iend = m_values.end(); i != iend; ++i)
	{
		std::string key_str = TextId::to_str(TextId::OBJ_VALS, to_underlying(i->first));
		if (!key_str.empty())
		{
			out << "V " << key_str << " " << i->second << "\n";
		}
	}

	return out.str();
}

void ObjVal::init_from_zone(const char *str)
{
	std::stringstream text(str);
	std::string key_str;
	int val = -1;

	if (text >> key_str >> val)
	{
		const int key = TextId::to_num(TextId::OBJ_VALS, key_str);
		if (key >= 0 && val >= 0)
		{
			m_values.emplace(static_cast<EValueKey>(key), val);
		}
		else
		{
			err_log("invalid key=%d or val=%d (%s %s:%d)",
				key, val, __func__, __FILE__, __LINE__);
		}
	}
}

bool is_valid_drinkcon(const ObjVal::EValueKey key)
{
	switch(key)
	{
	case ObjVal::EValueKey::POTION_SPELL1_NUM:
	case ObjVal::EValueKey::POTION_SPELL1_LVL:
	case ObjVal::EValueKey::POTION_SPELL2_NUM:
	case ObjVal::EValueKey::POTION_SPELL2_LVL:
	case ObjVal::EValueKey::POTION_SPELL3_NUM:
	case ObjVal::EValueKey::POTION_SPELL3_LVL:
	case ObjVal::EValueKey::POTION_PROTO_VNUM:
		return true;
	}
	return false;
}

void ObjVal::remove_incorrect_keys(int type)
{
	for (auto i = m_values.begin(); i != m_values.end(); /* empty */)
	{
		bool erased = false;
		switch(type)
		{
		case OBJ_DATA::ITEM_DRINKCON:
		case OBJ_DATA::ITEM_FOUNTAIN:
			if (!is_valid_drinkcon(i->first))
			{
				i = m_values.erase(i);
				erased = true;
			}
			break;
		} // switch

		if (!erased)
		{
			++i;
		}
	}
}

std::string print_obj_affects(const obj_affected_type &affect)
{
	sprinttype(affect.location, apply_types, buf2);
	bool negative = false;
	for (int j = 0; *apply_negative[j] != '\n'; j++)
	{
		if (!str_cmp(buf2, apply_negative[j]))
		{
			negative = true;
			break;
		}
	}
	if (!negative && affect.modifier < 0)
	{
		negative = true;
	}
	else if (negative && affect.modifier < 0)
	{
		negative = false;
	}

	sprintf(buf, "%s%s%s%s%s%d%s\r\n",
		KCYN, buf2, KNRM,
		KCYN, (negative ? " �������� �� " : " �������� �� "),
		abs(affect.modifier), KNRM);

	return std::string(buf);
}

void print_obj_affects(CHAR_DATA *ch, const obj_affected_type &affect)
{
	sprinttype(affect.location, apply_types, buf2);
	bool negative = false;
	for (int j = 0; *apply_negative[j] != '\n'; j++)
	{
		if (!str_cmp(buf2, apply_negative[j]))
		{
			negative = true;
			break;
		}
	}
	if (!negative && affect.modifier < 0)
	{
		negative = true;
	}
	else if (negative && affect.modifier < 0)
	{
		negative = false;
	}
	sprintf(buf, "   %s%s%s%s%s%d%s\r\n",
		CCCYN(ch, C_NRM), buf2, CCNRM(ch, C_NRM),
		CCCYN(ch, C_NRM),
		negative ? " �������� �� " : " �������� �� ", abs(affect.modifier), CCNRM(ch, C_NRM));
	send_to_char(buf, ch);
}

typedef std::map<OBJ_DATA::EObjectType, std::string> EObjectType_name_by_value_t;
typedef std::map<const std::string, OBJ_DATA::EObjectType> EObjectType_value_by_name_t;
EObjectType_name_by_value_t EObjectType_name_by_value;
EObjectType_value_by_name_t EObjectType_value_by_name;
void init_EObjectType_ITEM_NAMES()
{
	EObjectType_value_by_name.clear();
	EObjectType_name_by_value.clear();

	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_LIGHT] = "ITEM_LIGHT";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_SCROLL] = "ITEM_SCROLL";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_WAND] = "ITEM_WAND";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_STAFF] = "ITEM_STAFF";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_WEAPON] = "ITEM_WEAPON";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_FIREWEAPON] = "ITEM_FIREWEAPON";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_MISSILE] = "ITEM_MISSILE";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_TREASURE] = "ITEM_TREASURE";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_ARMOR] = "ITEM_ARMOR";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_POTION] = "ITEM_POTION";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_WORN] = "ITEM_WORN";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_OTHER] = "ITEM_OTHER";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_TRASH] = "ITEM_TRASH";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_TRAP] = "ITEM_TRAP";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_CONTAINER] = "ITEM_CONTAINER";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_NOTE] = "ITEM_NOTE";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_DRINKCON] = "ITEM_DRINKCON";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_KEY] = "ITEM_KEY";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_FOOD] = "ITEM_FOOD";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_MONEY] = "ITEM_MONEY";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_PEN] = "ITEM_PEN";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_BOAT] = "ITEM_BOAT";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_FOUNTAIN] = "ITEM_FOUNTAIN";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_BOOK] = "ITEM_BOOK";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_INGREDIENT] = "ITEM_INGREDIENT";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_MING] = "ITEM_MING";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_MATERIAL] = "ITEM_MATERIAL";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_BANDAGE] = "ITEM_BANDAGE";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_ARMOR_LIGHT] = "ITEM_ARMOR_LIGHT";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_ARMOR_MEDIAN] = "ITEM_ARMOR_MEDIAN";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_ARMOR_HEAVY] = "ITEM_ARMOR_HEAVY";
	EObjectType_name_by_value[OBJ_DATA::EObjectType::ITEM_ENCHANT] = "ITEM_ENCHANT";

	for (const auto& i : EObjectType_name_by_value)
	{
		EObjectType_value_by_name[i.second] = i.first;
	}
}

template <>
const std::string& NAME_BY_ITEM<OBJ_DATA::EObjectType>(const OBJ_DATA::EObjectType item)
{
	if (EObjectType_name_by_value.empty())
	{
		init_EObjectType_ITEM_NAMES();
	}
	return EObjectType_name_by_value.at(item);
}

template <>
OBJ_DATA::EObjectType ITEM_BY_NAME(const std::string& name)
{
	if (EObjectType_name_by_value.empty())
	{
		init_EObjectType_ITEM_NAMES();
	}
	return EObjectType_value_by_name.at(name);
}

typedef std::map<OBJ_DATA::EObjectMaterial, std::string> EObjectMaterial_name_by_value_t;
typedef std::map<const std::string, OBJ_DATA::EObjectMaterial> EObjectMaterial_value_by_name_t;
EObjectMaterial_name_by_value_t EObjectMaterial_name_by_value;
EObjectMaterial_value_by_name_t EObjectMaterial_value_by_name;
void init_EObjectMaterial_ITEM_NAMES()
{
	EObjectMaterial_value_by_name.clear();
	EObjectMaterial_name_by_value.clear();

	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_NONE] = "MAT_NONE";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_BULAT] = "MAT_BULAT";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_BRONZE] = "MAT_BRONZE";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_IRON] = "MAT_IRON";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_STEEL] = "MAT_STEEL";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_SWORDSSTEEL] = "MAT_SWORDSSTEEL";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_COLOR] = "MAT_COLOR";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_CRYSTALL] = "MAT_CRYSTALL";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_WOOD] = "MAT_WOOD";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_SUPERWOOD] = "MAT_SUPERWOOD";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_FARFOR] = "MAT_FARFOR";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_GLASS] = "MAT_GLASS";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_ROCK] = "MAT_ROCK";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_BONE] = "MAT_BONE";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_MATERIA] = "MAT_MATERIA";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_SKIN] = "MAT_SKIN";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_ORGANIC] = "MAT_ORGANIC";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_PAPER] = "MAT_PAPER";
	EObjectMaterial_name_by_value[OBJ_DATA::EObjectMaterial::MAT_DIAMOND] = "MAT_DIAMOND";

	for (const auto& i : EObjectMaterial_name_by_value)
	{
		EObjectMaterial_value_by_name[i.second] = i.first;
	}
}

template <>
const std::string& NAME_BY_ITEM<OBJ_DATA::EObjectMaterial>(const OBJ_DATA::EObjectMaterial item)
{
	if (EObjectMaterial_name_by_value.empty())
	{
		init_EObjectMaterial_ITEM_NAMES();
	}
	return EObjectMaterial_name_by_value.at(item);
}

template <>
OBJ_DATA::EObjectMaterial ITEM_BY_NAME(const std::string& name)
{
	if (EObjectMaterial_name_by_value.empty())
	{
		init_EObjectMaterial_ITEM_NAMES();
	}
	return EObjectMaterial_value_by_name.at(name);
}

namespace SetSystem {

	struct SetNode
	{
		// ������ ������ �� ����������� ���� ��� ������
		// ������ ���� ��� ��� ������ � ������ �� ��������
		std::set<int> set_vnum;
		// ������ ������ �� ������� ���� � �������� ����
		// ���� ����� ���������� � ������ ������ 1 �������
		// ������ ������� ��� ��� ������������ � ����
		std::vector<int> obj_vnum;
	};

	std::vector<SetNode> set_list;
	const unsigned BIG_SET_ITEMS = 9;
	// ��� �������� ��� ������� �����
	std::set<int> vnum_list;

	// * ���������� ������ ����-����� ��� ����������� ������.
	void init_set_list()
	{
		for (id_to_set_info_map::const_iterator i = OBJ_DATA::set_table.begin(),
			iend = OBJ_DATA::set_table.end(); i != iend; ++i)
		{
			if (i->second.size() > BIG_SET_ITEMS)
			{
				SetNode node;
				for (set_info::const_iterator k = i->second.begin(),
					kend = i->second.end(); k != kend; ++k)
				{
					node.set_vnum.insert(k->first);
				}
				set_list.push_back(node);
			}
		}
	}

	// * �������� ���� �� ���������� ������������ ����.
	void reset_set_list()
	{
		for (std::vector<SetNode>::iterator i = set_list.begin(),
			iend = set_list.end(); i != iend; ++i)
		{
			i->obj_vnum.clear();
		}
	}

	// * �������� ������ �� �������������� � ����� �� set_list.
	void check_item(int vnum)
	{
		for (std::vector<SetNode>::iterator i = set_list.begin(),
			iend = set_list.end(); i != iend; ++i)
		{
			std::set<int>::const_iterator k = i->set_vnum.find(vnum);
			if (k != i->set_vnum.end())
			{
				i->obj_vnum.push_back(vnum);
			}
		}
	}

	// * ��������� ������� ������ � ����� ��� ����.�����.
	void delete_item(int pt_num, int vnum)
	{
		bool need_save = false;
		// �����
		if (player_table[pt_num].timer)
		{
			for (std::vector<save_time_info>::iterator i = player_table[pt_num].timer->time.begin(),
				iend = player_table[pt_num].timer->time.end(); i != iend; ++i)
			{
				if (i->vnum == vnum)
				{
					log("[TO] Player %s : set-item %d deleted",
						player_table[pt_num].name, i->vnum);
					i->timer = -1;
					int rnum = real_object(i->vnum);
					if (rnum >= 0)
					{
						obj_proto.dec_stored(rnum);
					}
					need_save = true;
				}
			}
		}
		if (need_save)
		{
			if (!Crash_write_timer(pt_num))
			{
				log("SYSERROR: [TO] Error writing timer file for %s",
					player_table[pt_num].name);
			}
			return;
		}
		// ����.����
		Depot::delete_set_item(player_table[pt_num].unique, vnum);
	}

	// * �������� ��� ������ ���� ���� � ����.�������� �����.
	void check_rented()
	{
		init_set_list();

		for (int i = 0; i <= top_of_p_table; i++)
		{
			reset_set_list();
			// �����
			if (player_table[i].timer)
			{
				for (std::vector<save_time_info>::iterator it = player_table[i].timer->time.begin(),
					it_end = player_table[i].timer->time.end(); it != it_end; ++it)
				{
					if (it->timer >= 0)
					{
						check_item(it->vnum);
					}
				}
			}
			// ����.����
			Depot::check_rented(player_table[i].unique);
			// �������� ��������� ������
			for (std::vector<SetNode>::iterator it = set_list.begin(),
				iend = set_list.end(); it != iend; ++it)
			{
				if (it->obj_vnum.size() == 1)
				{
					delete_item(i, it->obj_vnum[0]);
				}
			}
		}
	}

#define MINI_SET_ITEMS 3

	/**
	* �����, �����.
	* �������� ����� �� BIG_SET_ITEMS � ����� ��������� �� �����������.
	*/
	bool is_big_set(const CObjectPrototype* obj, bool is_mini)
	{
		unsigned int sets_items = is_mini ? MINI_SET_ITEMS : BIG_SET_ITEMS;
		if (!obj->get_extra_flag(EExtraFlag::ITEM_SETSTUFF))
		{
			return false;
		}
		for (id_to_set_info_map::const_iterator i = OBJ_DATA::set_table.begin(),
			iend = OBJ_DATA::set_table.end(); i != iend; ++i)
		{
			if (i->second.find(GET_OBJ_VNUM(obj)) != i->second.end()
				&& i->second.size() > sets_items)
			{
				return true;
			}
		}
		return false;
	}
	bool find_set_item(OBJ_DATA *obj)
	{
		for (; obj; obj = obj->get_next_content())
		{
			std::set<int>::const_iterator i = vnum_list.find(GET_OBJ_VNUM(obj));
			if (i != vnum_list.end())
			{
				return true;
			}

			if (find_set_item(obj->get_contains()))
			{
				return true;
			}
		}
		return false;
	}

	// * ��������� ������ ����� �� ���� �� ������, ��� � vnum (�������� �� ����).
	void init_vnum_list(int vnum)
	{
		vnum_list.clear();
		for (id_to_set_info_map::const_iterator i = OBJ_DATA::set_table.begin(),
			iend = OBJ_DATA::set_table.end(); i != iend; ++i)
		{
			if (i->second.find(vnum) != i->second.end())
				//&& i->second.size() > BIG_SET_ITEMS)
			{
				for (set_info::const_iterator k = i->second.begin(),
					kend = i->second.end(); k != kend; ++k)
				{
					if (k->first != vnum)
					{
						vnum_list.insert(k->first);
					}
				}
			}
		}

		if (vnum_list.empty())
		{
			vnum_list = obj_sets::vnum_list_add(vnum);
		}
	}

	/**
	* ����������, ���������, �������, ����. ����.
	* ��������� ������� ���� � ����� ���������, ���� ������ �� �������� ����.
	* ����. ����, �����.
	*/
	bool is_norent_set(CHAR_DATA *ch, OBJ_DATA *obj)
	{
		if (!obj->get_extra_flag(EExtraFlag::ITEM_SETSTUFF))
		{
			return false;
		}

		init_vnum_list(GET_OBJ_VNUM(obj));

		if (vnum_list.empty())
		{
			return false;
		}

		// ����������
		for (int i = 0; i < NUM_WEARS; ++i)
		{
			if (find_set_item(GET_EQ(ch, i)))
			{
				return false;
			}
		}
		// ���������
		if (find_set_item(ch->carrying))
		{
			return false;
		}
		// �������
		if (ch->followers)
		{
			for (struct follow_type *k = ch->followers; k; k = k->next)
			{
				if (!IS_CHARMICE(k->follower) || !k->follower->master)
				{
					continue;
				}
				for (int j = 0; j < NUM_WEARS; j++)
				{
					if (find_set_item(GET_EQ(k->follower, j)))
					{
						return false;
					}
				}
				if (find_set_item(k->follower->carrying))
				{
					return false;
				}
			}
		}
		// ����. ���������
		if (Depot::find_set_item(ch, vnum_list))
		{
			return false;
		}

		return true;
	}

} // namespace SetSystem

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
