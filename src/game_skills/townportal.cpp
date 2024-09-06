#include "townportal.h"

#include "modify.h"
#include "handler.h"
#include "game_fight/pk.h"
#include "game_magic/magic_rooms.h"
#include "utils/table_wrapper.h"

#include <third_party_libs/fmt/include/fmt/format.h>

class Townportal {
 public:
  enum class State { kEnabled, kDisabled, kForbidden };

  Townportal() = default;
  Townportal(std::string_view name,
			 RoomVnum room_vnum,
			 int min_char_level,
			 State state = State::kEnabled)
	  : name_(name),
		room_vnum_(room_vnum),
		min_char_level_(min_char_level),
		state(state) {}

  [[nodiscard]] std::string_view GetName() const { return name_; };
  [[nodiscard]] RoomVnum GetRoomVnum() const { return room_vnum_; };
  [[nodiscard]] int GetMinCharLevel() const { return min_char_level_; };
  [[nodiscard]] bool IsEnabled() const { return (state == State::kEnabled); };
  [[nodiscard]] bool IsDisabled() const { return (state != State::kEnabled); };
  [[nodiscard]] bool IsAllowed() const { return (state != State::kForbidden); };
  [[nodiscard]] bool IsForbidden() const { return (state == State::kForbidden); };
  void SetEnabled(bool enabled);

 private:
  std::string name_;
  RoomVnum room_vnum_{0};
  int min_char_level_{0};
  State state{State::kEnabled};
};

void Townportal::SetEnabled(bool enabled) {
	if (state != State::kForbidden) {
		state = enabled ? State::kEnabled : State::kDisabled;
	}
}

Townportal incorrect_townportal("undefined", kNowhere, kMaxPlayerLevel + 1, Townportal::State::kForbidden);
std::vector<Townportal> townportal_roster;

void TryOpenTownportal(CharData *ch, const Townportal &portal);
void TryOpenLabelPortal(CharData *ch, char *argument);
void OpenTownportal(CharData *ch, const Townportal &portal);
void ForgetTownportal(CharData *ch, char *argument);
void ListKnownTownportalsToChar(CharData *ch);
void SetSkillTownportalTimer(CharData *ch);
Townportal &FindTownportal(RoomVnum vnum);
Townportal &FindTownportal(std::string_view name);
Townportal GetLabelPortal(CharData *ch);
bool IsPortalKnown(CharData *ch, RoomVnum vnum);
std::size_t CalcMaxPortals(CharData *ch);
int CalcMinPortalLevel(CharData *ch, RoomRnum vnum);

namespace one_way_portal {

void ReplacePortalTimer(CharData *ch, RoomRnum from_room, RoomRnum to_room, int time) {
//	sprintf(buf, "Ставим портал из %d в %d", world[from_room]->vnum, world[to_room]->vnum);
//	mudlog(buf, CMP, kLvlImmortal, SYSLOG, true);

	Affect<room_spells::ERoomApply> af;
	af.type = ESpell::kPortalTimer;
	af.bitvector = 0;
	af.duration = time; //раз в 2 секунды
	af.modifier = to_room;
	af.battleflag = 0;
	af.location = room_spells::ERoomApply::kNone;
	af.caster_id = ch ? GET_ID(ch) : 0;
	af.must_handled = false;
	af.apply_time = 0;
//	room_spells::AffectRoomJoinReplace(world[from_room], af);
	room_spells::affect_to_room(world[from_room], af);
	room_spells::AddRoomToAffected(world[from_room]);
	af.modifier = from_room;
	af.bitvector = room_spells::ERoomAffect::kNoPortalExit;
	room_spells::affect_to_room(world[to_room], af);
//	room_spells::AffectRoomJoinReplace(world[to_room], af);
	room_spells::AddRoomToAffected(world[to_room]);
}

} // namespace OneWayPortal

inline void DecayPortalMessage(const RoomRnum room_num) {
	act("Пентаграмма медленно растаяла.", false, world[room_num]->first_character(), nullptr, nullptr, kToRoom);
	act("Пентаграмма медленно растаяла.", false, world[room_num]->first_character(), nullptr, nullptr, kToChar);
}

void GoTownportal(CharData *ch, char *argument) {
	auto portal = FindTownportal(argument);
	if (portal.IsAllowed() && IsPortalKnown(ch, portal.GetRoomVnum())) {
		TryOpenTownportal(ch, portal);
	} else {
		TryOpenLabelPortal(ch, argument);
	}
}

void TryOpenLabelPortal(CharData *ch, char *argument) {
	if (name_cmp(ch, argument)) {
		const auto &portal = GetLabelPortal(ch);
		if (portal.IsAllowed()) {
			TryOpenTownportal(ch, portal);
		} else {
			SendMsgToChar("Вы не оставляли рунных меток.\r\n", ch);
		}
		return;
	}
	SendMsgToChar("Вам неизвестны такие руны.\r\n", ch);
}

Townportal GetLabelPortal(CharData *ch) {
	auto label_room = room_spells::FindAffectedRoomByCasterID(GET_ID(ch), ESpell::kRuneLabel);
	if (label_room) {
		return {ch->get_name(), label_room->vnum, 1};
	} else {
		return {"undefined", kNowhere, 1, Townportal::State::kForbidden};
	}
}

void TryOpenTownportal(CharData *ch, const Townportal &portal) {
	if (IsTimedBySkill(ch, ESkill::kTownportal)) {
		SendMsgToChar("У вас недостаточно сил для постановки врат.\r\n", ch);
		return;
	}

	if (FindTownportal(GET_ROOM_VNUM(ch->in_room)).IsAllowed()) {
		SendMsgToChar("Камень рядом с вами мешает вашей магии.\r\n", ch);
		return;
	}

	if (room_spells::IsRoomAffected(world[ch->in_room], ESpell::kRuneLabel)) {
		SendMsgToChar("Начертанные на земле магические руны подавляют вашу магию!\r\n", ch);
		return;
	}

	if (ROOM_FLAGGED(ch->in_room, ERoomFlag::kNoMagic) && !IS_GRGOD(ch)) {
		SendMsgToChar("Ваша магия потерпела неудачу и развеялась по воздуху.\r\n", ch);
		act("Магия $n1 потерпела неудачу и развеялась по воздуху.", false, ch, nullptr, nullptr, kToRoom);
		return;
	}

	if (portal.IsDisabled()) {
		act("Лазурная пентаграмма возникла в воздухе... и сразу же растаяла.", false, ch, nullptr, nullptr, kToChar);
		act("$n сложил$g руки в молитвенном жесте, испрашивая у Богов врата...", false, ch, nullptr, nullptr, kToRoom);
		act("Лазурная пентаграмма возникла в воздухе... и сразу же растаяла.", false, ch, nullptr, nullptr, kToRoom);
		SetSkillTownportalTimer(ch);
		return;
	}

	OpenTownportal(ch, portal);
}

void OpenTownportal(CharData *ch, const Townportal &portal) {
	ImproveSkill(ch, ESkill::kTownportal, 1, nullptr);
	RoomData *from_room = world[ch->in_room];
	auto to_room = GetRoomRnum(portal.GetRoomVnum());
	from_room->pkPenterUnique = 0;
	one_way_portal::ReplacePortalTimer(ch, ch->in_room, to_room, 29);
	act("Лазурная пентаграмма возникла в воздухе.", false, ch, nullptr, nullptr, kToChar);
	act("$n сложил$g руки в молитвенном жесте, испрашивая у Богов врата...", false, ch, nullptr, nullptr, kToRoom);
	act("Лазурная пентаграмма возникла в воздухе.", false, ch, nullptr, nullptr, kToRoom);
	SetSkillTownportalTimer(ch);
}

void SetSkillTownportalTimer(CharData *ch) {
	if (!IS_IMMORTAL(ch)) {
		TimedSkill timed;
		timed.skill = ESkill::kTownportal;
		// timed.time - это unsigned char, поэтому при уходе в минус будет вынос на 255 и ниже
		int modif = ch->GetSkill(ESkill::kTownportal) / 7 + number(1, 5);
		timed.time = MAX(1, 25 - modif);
		ImposeTimedSkill(ch, &timed);
	}
}

void ListKnownTownportalsToChar(CharData *ch) {
	std::ostringstream out;
	if (ch->player_specials->townportals.empty()) {
		out << " В настоящий момент вам неизвестны врата.\r\n";
	} else {
		out << " Вам доступны следующие врата:\r\n";
		table_wrapper::Table table;
		const int columns_num{3};
		int count = 1;
		const auto &player_portals = ch->player_specials->townportals;
		for (const auto it : player_portals) {
			auto portal = FindTownportal(it);
			if (portal.IsAllowed()) {
				table << portal.GetName();
				if (count % columns_num == 0) {
					table << table_wrapper::kEndRow;
				}
				++count;
			}
		}
		table_wrapper::DecorateSimpleTable(ch, table);
		table_wrapper::PrintTableToStream(out, table);
		out << fmt::format("\r\n Сейчас в памяти {}.\r\n", player_portals.size());
	}
	out << fmt::format(" Максимально возможно {}.\r\n", CalcMaxPortals(ch));

	page_string(ch->desc, out.str());
}

void DoTownportal(CharData *ch, char *argument, int/* cmd*/, int/* subcmd*/) {
	if (ch->IsNpc() || !ch->GetSkill(ESkill::kTownportal)) {
		SendMsgToChar("Прежде изучите секрет постановки врат.\r\n", ch);
		return;
	}

	if (!argument || !*argument) {
		ListKnownTownportalsToChar(ch);
		return;
	}

	char arg2[kMaxInputLength];
	two_arguments(argument, arg, arg2);
	if (!str_cmp(arg, "забыть")) {
		ForgetTownportal(ch, arg2);
		return;
	}

	GoTownportal(ch, arg);
}

void ForgetTownportal(CharData *ch, char *argument) {
	const auto &portal = FindTownportal(argument);
	auto predicate = [portal](const RoomVnum p) { return p == portal.GetRoomVnum(); };
	auto erased = std::erase_if(ch->player_specials->townportals, predicate);
	if (erased) {
		auto msg = fmt::format("Вы полностью забыли, как выглядит камень '&R{}&n'.\r\n", argument);
		SendMsgToChar(msg, ch);
	} else {
		SendMsgToChar("Чтобы забыть что-нибудь ненужное, следует сперва изучить что-нибудь ненужное...", ch);
	}
}

void LoadTownportals() {
	FILE *portal_f;
	char nm[300], nm2[300], *wrd;
	int rnm = 0, i, level = 0;

	townportal_roster.clear();

	if (!(portal_f = fopen(LIB_MISC "portals.lst", "r"))) {
		log("Cannot open portals.lst");
		return;
	}

	while (get_line(portal_f, nm)) {
		if (!nm[0] || nm[0] == ';')
			continue;
		sscanf(nm, "%d %d %s", &rnm, &level, nm2);
		if (GetRoomRnum(rnm) == kNowhere || nm2[0] == '\0') {
			log("Invalid portal entry detected");
			continue;
		}
		wrd = nm2;
		for (i = 0; !(i == 10 || wrd[i] == ' ' || wrd[i] == '\0'); i++);
		wrd[i] = '\0';

		townportal_roster.emplace_back(wrd, rnm, level);
	}
	fclose(portal_f);
}

Townportal &FindTownportal(RoomVnum vnum) {
	auto predicate = [vnum](const Townportal &p) { return p.GetRoomVnum() == vnum; };
	auto it = std::find_if(townportal_roster.begin(), townportal_roster.end(), predicate);
	if (it != townportal_roster.end()) {
		return *it.base();
	}

	return incorrect_townportal;
}

Townportal &FindTownportal(std::string_view name) {
	auto predicate = [name](const Townportal &p) { return p.GetName() == name; };
	auto it = std::find_if(townportal_roster.begin(), townportal_roster.end(), predicate);
	if (it != townportal_roster.end()) {
		return *it.base();
	}

	return incorrect_townportal;
}

int CalcMinPortalLevel(CharData *ch, RoomRnum vnum) {
	auto predicate = [vnum](const Townportal &p) { return p.GetRoomVnum() == vnum; };
	auto it = std::find_if(townportal_roster.begin(), townportal_roster.end(), predicate);
	if (it != townportal_roster.end()) {
		return std::max(1, it->GetMinCharLevel() - GetRealRemort(ch) / 2);
	}

	return kMaxPlayerLevel + 1;
}

void AddTownportalToChar(CharData *ch, RoomVnum vnum) {
	if (FindTownportal(vnum).IsAllowed()) {
		std::erase_if(ch->player_specials->townportals, [vnum](RoomVnum p) { return p == vnum; });
		ch->player_specials->townportals.push_back(vnum);
	}
}

bool IsPortalKnown(CharData *ch, RoomVnum vnum) {
	auto &char_townportals = ch->player_specials->townportals;
	const auto it = std::find(char_townportals.begin(), char_townportals.end(), vnum);
	return (it != char_townportals.end());
}

// Убирает лишние и несуществующие порталы у чара
void CleanupSurplusPortals(CharData *ch) {
	auto &char_portals = ch->player_specials->townportals;
	auto predicate = [](const RoomVnum portal_vnum) { return FindTownportal(portal_vnum).IsForbidden(); };
	std::erase_if(ch->player_specials->townportals, predicate);
	auto max_portals = CalcMaxPortals(ch);
	if (char_portals.size() > max_portals) {
		char_portals.resize(max_portals);
	}
}

bool ViewTownportal(CharData *ch, int where_bits) {
	if (ch->GetSkill(ESkill::kTownportal) == 0) {
		return false;
	}
	auto portal = FindTownportal(GET_ROOM_VNUM(ch->in_room));
	if (portal.IsAllowed() && IS_SET(where_bits, EFind::kObjRoom)) {
		auto room_vnum = GET_ROOM_VNUM(ch->in_room);
		if (IsPortalKnown(ch, room_vnum)) {
			auto msg = fmt::format("На камне огненными буквами написано слово '&R{}&n'.\r\n", portal.GetName());
			SendMsgToChar(msg, ch);
			return true;
		} else if (GetRealLevel(ch) < CalcMinPortalLevel(ch, room_vnum)) {
			SendMsgToChar("На камне что-то написано огненными буквами.\r\n", ch);
			SendMsgToChar("Но вы еще недостаточно искусны, чтобы разобрать слово.\r\n", ch);
			return true;
		} else {
			if (ch->player_specials->townportals.size() >= CalcMaxPortals(ch)) {
				SendMsgToChar
					("Все доступные вам камни уже запомнены, удалите и попробуйте еще.\r\n", ch);
				return true;
			}
			auto msg = fmt::format("На камне огненными буквами написано слово '&R{}&n'.\r\n", portal.GetName());
			SendMsgToChar(msg, ch);
			AddTownportalToChar(ch, portal.GetRoomVnum());
			CleanupSurplusPortals(ch);
			return true;
		}
	}
	return false;
}

void ShowPortalRunestone(CharData *ch) {
	if (ch->GetSkill(ESkill::kTownportal)) {
		const auto &portal = FindTownportal(GET_ROOM_VNUM(ch->in_room));
		if (portal.IsAllowed()) {
			if (portal.IsEnabled()) {
				SendMsgToChar("Рунный камень с изображением пентаграммы немного выступает из земли.\r\n", ch);
			} else {
				SendMsgToChar("Рунный камень с изображением пентаграммы немного выступает из земли... расколот.\r\n", ch);
			}
		}
	}
}

std::size_t CalcMaxPortals(CharData *ch)  {
	return GetRealLevel(ch)/3 + GetRealRemort(ch);
}

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
