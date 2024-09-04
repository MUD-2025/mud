/* ************************************************************************
*   File: ban.cpp                                       Part of Bylins    *
*  Usage: banning/unbanning/checking sites and player names               *
*                                                                         *
*  $Author$                                                        *
*  $Date$                                           *
*  $Revision$                                                       *
************************************************************************ */

#include "ban.h"
#include "modify.h"
#include "structs/global_objects.h"

#include <third_party_libs/fmt/include/fmt/format.h>

extern DescriptorData *descriptor_list;

void do_ban(CharData *ch, char *argument, int cmd, int subcmd);
void do_unban(CharData *ch, char *argument, int cmd, int subcmd);
int Valid_Name(char *newname);
void ReadCharacterInvalidNamesList(void);

void do_ban(CharData *ch, char *argument, int/* cmd*/, int/* subcmd*/) {
	if (!*argument) {
		ban->ShowBannedIp(BanList::SORT_BY_DATE, ch);
		return;
	}

	char flag[kMaxInputLength], site[kMaxInputLength];
	argument = two_arguments(argument, flag, site);

	if (!str_cmp(flag, "proxy")) {
		if (!*site) {
			ban->ShowBannedProxy(BanList::SORT_BY_NAME, ch);
			return;
		}
		if (site[0] == '-')
			switch (site[1]) {
				case 'n':
				case 'N': ban->ShowBannedProxy(BanList::SORT_BY_BANNER, ch);
					return;
				case 'i':
				case 'I': ban->ShowBannedProxy(BanList::SORT_BY_NAME, ch);
					return;
				default: SendMsgToChar("Usage: ban proxy [[-N | -I] | ip] \r\n", ch);
					SendMsgToChar(" -N : Sort by banner name \r\n", ch);
					SendMsgToChar(" -I : Sort by banned ip \r\n", ch);
					return;
			};
		std::string banned_ip(site);
		std::string banner_name(GET_NAME(ch));

		if (!ban->add_proxy_ban(banned_ip, banner_name)) {
			SendMsgToChar("The site is already in the proxy ban list.\r\n", ch);
			return;
		}
		SendMsgToChar("Proxy banned.\r\n", ch);
		return;
	}

	if (!*site && flag[0] == '-')
		switch (flag[1]) {
			case 'n':
			case 'N': ban->ShowBannedIp(BanList::SORT_BY_BANNER, ch);
				return;
			case 'd':
			case 'D': ban->ShowBannedIp(BanList::SORT_BY_DATE, ch);
				return;
			case 'i':
			case 'I': ban->ShowBannedIp(BanList::SORT_BY_NAME, ch);
				return;
			default:;
		};

	if (!*flag || !*site) {
		SendMsgToChar("Usage: ban [[-N | -D | -I] | {all | select | new } ip duration [reason]] \r\n", ch);
		SendMsgToChar("or\r\n", ch);
		SendMsgToChar(" ban proxy [[-N | -I] | ip] \r\n", ch);
		SendMsgToChar(" -N : Sort by banner name \r\n", ch);
		SendMsgToChar(" -D : Sort by ban date \r\n", ch);
		SendMsgToChar(" -I : Sort by bannd ip \r\n", ch);
		return;
	}

	if (!(!str_cmp(flag, "select") || !str_cmp(flag, "all")
		|| !str_cmp(flag, "new"))) {
		SendMsgToChar("Flag must be ALL, SELECT, or NEW.\r\n", ch);
		return;
	}

	char length[kMaxInputLength], *reason;
	int len, ban_type = BanList::BAN_ALL;
	reason = one_argument(argument, length);
	skip_spaces(&reason);
	len = atoi(length);
	if (!*length || len == 0) {
		SendMsgToChar("Usage: ban {all | select | new } ip duration [reason]\r\n", ch);
		return;
	}
	std::string banned_ip(site);
	std::string banner_name(GET_NAME(ch));
	std::string ban_reason(reason);
	for (int i = BanList::BAN_NEW; i <= BanList::BAN_ALL; i++)
		if (!str_cmp(flag, BanList::ban_types[i]))
			ban_type = i;

	if (!ban->add_ban(banned_ip, ban_reason, banner_name, len, ban_type)) {
		SendMsgToChar("That site has already been banned -- unban it to change the ban type.\r\n", ch);
		return;
	}
	SendMsgToChar("Site banned.\r\n", ch);
}

void do_unban(CharData *ch, char *argument, int/* cmd*/, int/* subcmd*/) {
	char site[kMaxInputLength];
	one_argument(argument, site);
	if (!*site) {
		SendMsgToChar("A site to unban might help.\r\n", ch);
		return;
	}
	std::string unban_site(site);
	if (!ban->unban(unban_site, ch)) {
		SendMsgToChar("The site is not currently banned.\r\n", ch);
		return;
	}
}


/**************************************************************************
 *  Code to check for invalid names (i.e., profanity, etc.)		  *
 *  Written by Sharon P. Goza						  *
 **************************************************************************/

#define MAX_INVALID_NAMES    200

char *invalid_list[MAX_INVALID_NAMES];
int num_invalid = 0;

int Is_Valid_Name(char *newname) {
	int i;
	char tempname[kMaxInputLength];

	if (!*invalid_list || num_invalid < 1)
		return (1);

	// change to lowercase
	strcpy(tempname, newname);
	for (i = 0; tempname[i]; i++)
		tempname[i] = LOWER(tempname[i]);

	// Does the desired name contain a string in the invalid list?
	for (i = 0; i < num_invalid; i++)
		if (strstr(tempname, invalid_list[i]))
			return (0);

	return (1);
}

int Is_Valid_Dc(char *newname) {
	DescriptorData *dt;

	for (dt = descriptor_list; dt; dt = dt->next)
		if (dt->character && GET_NAME(dt->character)
			&& !str_cmp(GET_NAME(dt->character), newname))
			return (0);
	return (1);
}

int Valid_Name(char *newname) {
	return (Is_Valid_Name(newname) && Is_Valid_Dc(newname));
}

void ReadCharacterInvalidNamesList(void) {
	FILE *fp;
	char temp[256];

	if (!(fp = fopen(XNAME_FILE, "r"))) {
		perror("SYSERR: Unable to open '" XNAME_FILE "' for reading");
		return;
	}

	num_invalid = 0;
	while (get_line(fp, temp) && num_invalid < MAX_INVALID_NAMES)
		invalid_list[num_invalid++] = str_dup(temp);

	if (num_invalid >= MAX_INVALID_NAMES) {
		log("SYSERR: Too many invalid names; change MAX_INVALID_NAMES in ban.c");
		exit(1);
	}

	fclose(fp);
}

// Тут все, что связано с регистрацией клубов и т.п.

// конвертация строкового ip в число для сравнений
unsigned long TxtToIp(const char *text) {
	int ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0;
	unsigned long ip = 0;

	sscanf(text, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4);
	ip = (ip1 << 24) + (ip2 << 16) + (ip3 << 8) + ip4;
	return ip;
}

// тоже для строкового аргумента
unsigned long TxtToIp(std::string &text) {
	int ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0;
	unsigned long ip = 0;

	sscanf(text.c_str(), "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4);
	ip = (ip1 << 24) + (ip2 << 16) + (ip3 << 8) + ip4;
	return ip;
}

// тоже самое, только обратно для визуального отображения
std::string IpToTxt(unsigned long ip) {
	char text[32];
	sprintf(text, "%lu.%lu.%lu.%lu", ip >> 24, ip >> 16 & 0xff, ip >> 8 & 0xff, ip & 0xff);
	std::string buffer = text;
	return buffer;
}

#define MAX_PROXY_CONNECT 50

struct ProxyIp {
	unsigned long ip2;   // конечный ип в случае диапазона
	int num;             // кол-во максимальных коннектов
	std::string text;    // комментарий
	std::string textIp;  // ип в виде строки
	std::string textIp2; // конечный ип в виде строки
};

typedef std::shared_ptr<ProxyIp> ProxyIpPtr;
typedef std::map<unsigned long, ProxyIpPtr> ProxyListType;
// собственно список ип и кол-ва коннектов
static ProxyListType proxyList;

// сохранение списка прокси в файл (ип в текстовом виде, шоб менять можно было в файле)
void SaveProxyList() {
	std::ofstream file(PROXY_FILE);
	if (!file.is_open()) {
		log("Error open file: %s! (%s %s %d)", PROXY_FILE, __FILE__, __func__, __LINE__);
		return;
	}

	for (auto it = proxyList.begin(); it != proxyList.end(); ++it) {
		file << it->second->textIp << "  " << (it->second->textIp2.empty() ? "0"
																		   : it->second->textIp2) << "  "
			 << it->second->num << "  " << it->second->text << "\n";
	}
	file.close();
}

// лоад списка ip и максимально разрешенных коннектов с них
void LoadProxyList() {
	// если релоадим
	proxyList.clear();

	std::ifstream file(PROXY_FILE);
	if (!file.is_open()) {
		log("Error open file: %s! (%s %s %d)", PROXY_FILE, __FILE__, __func__, __LINE__);
		return;
	}

	std::string buffer, textIp, textIp2;
	int num = 0;

	while (file) {
		file >> textIp >> textIp2 >> num;
		std::getline(file, buffer);
		utils::Trim(buffer);
		if (textIp.empty() || textIp2.empty() || buffer.empty() || num < 2 || num > MAX_PROXY_CONNECT) {
			log("Error read file: %s! IP: %s IP2: %s Num: %d Text: %s (%s %s %d)", PROXY_FILE, textIp.c_str(),
				textIp2.c_str(), num, buffer.c_str(), __FILE__, __func__, __LINE__);
			// не стоит грузить файл, если там чет не то - похерится потом при сохранении
			// хотя будет смешно, если после неудачного лоада кто-то добавит запись в онлайне Ж)
			proxyList.clear();
			return;
		}

		ProxyIpPtr tempIp(new ProxyIp);
		tempIp->num = num;
		tempIp->text = buffer;
		tempIp->textIp = textIp;
		// 0 тут значит, что это не диапазон, чтобы не портить вид списка в маде - лучше пустую строку
		if (textIp2 != "0")
			tempIp->textIp2 = textIp2;
		unsigned long ip = TxtToIp(textIp);
		tempIp->ip2 = TxtToIp(textIp2);
		proxyList[ip] = tempIp;
	}
	file.close();
	SaveProxyList();
}

// проверка на присутствие в списке зарегистрированных ip и кол-ва соединений
// 0 - нету, 1 - в маде уже макс. число коннектов с данного ip, 2 - все ок
int CheckProxy(DescriptorData *ch) {
	//сначала ищем в списке, зачем зря коннекты считать
	ProxyListType::const_iterator it;
	// мысль простая - есть второй ип, знач смотрим диапазон, нету - знач сверяем на равенство ип
	for (it = proxyList.begin(); it != proxyList.end(); ++it) {
		if (!it->second->ip2) {
			if (it->first == ch->ip)
				break;
		} else if ((it->first <= ch->ip) && (ch->ip <= it->second->ip2))
			break;
	}
	if (it == proxyList.end())
		return 0;

	// терь можно и посчитать
	DescriptorData *i;
	int num_ip = 0;
	for (i = descriptor_list; i; i = i->next)
		if (i != ch && i->character && !IS_IMMORTAL(i->character) && (i->ip == ch->ip))
			num_ip++;

	// проверяем на кол-во коннектов
	if (it->second->num <= num_ip)
		return 1;

	return 2;
}

// команда proxy
void do_proxy(CharData *ch, char *argument, int/* cmd*/, int/* subcmd*/) {
	std::string buffer = argument, buffer2;
	GetOneParam(buffer, buffer2);

	if (CompareParam(buffer2, "list") || CompareParam(buffer2, "список")) {
		std::ostringstream out;
		out << "Формат списка: IP | IP2 | Максимум соединений | Комментарий\r\n";

		for (const auto &it : proxyList) {
			out << fmt::format(" {:<15}   {:<15}   {:<2}   {}\r\n",
							   it.second->textIp, it.second->textIp2, it.second->num, it.second->text);
		}
		page_string(ch->desc, out.str());

	} else if (CompareParam(buffer2, "add") || CompareParam(buffer2, "добавить")) {
		GetOneParam(buffer, buffer2);
		if (buffer2.empty()) {
			SendMsgToChar("Укажите регистрируемый IP адрес или параметр mask|маска.\r\n", ch);
			return;
		}
		// случай добавления диапазона ип
		std::string textIp, textIp2;
		if (CompareParam(buffer2, "mask") || CompareParam(buffer2, "маска")) {
			GetOneParam(buffer, buffer2);
			if (buffer2.empty()) {
				SendMsgToChar("Укажите начало диапазона.\r\n", ch);
				return;
			}
			textIp = buffer2;
			// должен быть второй ип
			GetOneParam(buffer, buffer2);
			if (buffer2.empty()) {
				SendMsgToChar("Укажите конец диапазона.\r\n", ch);
				return;
			}
			textIp2 = buffer2;
		} else // если это не диапазон и не пустая строка - значит это простой ип
			textIp = buffer2;

		GetOneParam(buffer, buffer2);
		if (buffer2.empty()) {
			SendMsgToChar("Укажите максимальное кол-во коннектов.\r\n", ch);
			return;
		}
		int num = atoi(buffer2.c_str());
		if (num < 2 || num > MAX_PROXY_CONNECT) {
			SendMsgToChar(ch, "Некорректное число коннектов (2-%d).\r\n", MAX_PROXY_CONNECT);
			return;
		}

		utils::TrimIf(buffer, " \'");
		if (buffer.empty()) {
			SendMsgToChar("Укажите причину регистрации.\r\n", ch);
			return;
		}
		char timeBuf[11];
		time_t now = time(0);
		strftime(timeBuf, sizeof(timeBuf), "%d/%m/%Y", localtime(&now));

		ProxyIpPtr tempIp(new ProxyIp);
		tempIp->text = buffer + " [" + GET_NAME(ch) + " " + timeBuf + "]";
		tempIp->num = num;
		tempIp->textIp = textIp;
		tempIp->textIp2 = textIp2;
		unsigned long ip = TxtToIp(textIp);
		unsigned long ip2 = TxtToIp(textIp2);
		if (ip2 && ((ip2 - ip) > 65535)) {
			SendMsgToChar("Слишком широкий диапазон. (Максимум x.x.0.0. до x.x.255.255).\r\n", ch);
			return;
		}
		tempIp->ip2 = ip2;
		proxyList[ip] = tempIp;
		SaveProxyList();
		SendMsgToChar("Запись добавлена.\r\n", ch);

	} else if (CompareParam(buffer2, "remove") || CompareParam(buffer2, "удалить")) {
		GetOneParam(buffer, buffer2);
		if (buffer2.empty()) {
			SendMsgToChar("Укажите удаляемый IP адрес или начало диапазона.\r\n", ch);
			return;
		}
		unsigned long ip = TxtToIp(buffer2);
		ProxyListType::iterator it;
		it = proxyList.find(ip);
		if (it == proxyList.end()) {
			SendMsgToChar("Данный IP/диапазон и так не зарегистрирован.\r\n", ch);
			return;
		}
		proxyList.erase(it);
		SaveProxyList();
		SendMsgToChar("Запись удалена.\r\n", ch);

	} else
		SendMsgToChar("Формат: proxy <list|список>\r\n"
					 "        proxy <add|добавить> ip <количество коннектов> комментарий\r\n"
					 "        proxy <add|добавить> <mask|маска> <начало диапазона> <конец диапазона> <количество коннектов> комментарий\r\n"
					 "        proxy <remove|удалить> ip\r\n", ch);
}

//////////////////////////////////////////////////////////////////////////////

const char *const BanList::ban_filename = "etc/badsites";
const char *const BanList::proxy_ban_filename = "etc/badproxy";
const char *BanList::ban_types[] = {
	"no",
	"new",
	"select",
	"all",
	"ERROR"
};

////////////////////////////////////////////////////////////////////////////////
bool BanList::ban_compare(BanNodePtr nodePtr, int mode, const void *op2) {
	switch (mode) {
		case BAN_IP_COMPARE:
			if (nodePtr->BannedIp == *(std::string *) op2)
				return true;
			break;
		case BAN_TIME_COMPARE:
			if (nodePtr->BanDate == *(long *) op2)
				return true;
			break;
		default:;
	}

	return false;
}

bool BanList::proxy_ban_compare(ProxyBanNodePtr nodePtr, int mode, const void *op2) {
	switch (mode) {
		case BAN_IP_COMPARE:
			if (nodePtr->BannedIp == *(std::string *) op2)
				return true;
			break;
		default:;
	}

	return false;
}

bool BanList::ban_sort_func(const BanNodePtr &lft, const BanNodePtr &rght, int sort_algorithm) {
	switch (sort_algorithm) {
		case SORT_BY_NAME:
			return std::lexicographical_compare(lft->BannedIp.begin(),
												lft->BannedIp.end(), rght->BannedIp.begin(), rght->BannedIp.end());
		case SORT_BY_DATE: return (lft->BanDate > rght->BanDate);
		case SORT_BY_BANNER:
			return std::lexicographical_compare(lft->BannerName.begin(),
												lft->BannerName.end(),
												rght->BannerName.begin(), rght->BannerName.end());
		default: return true;
	}
}

bool BanList::proxy_ban_sort_func(const ProxyBanNodePtr &lft, const ProxyBanNodePtr &rght, int sort_algorithm) {
	switch (sort_algorithm) {
		case SORT_BY_NAME:
			return std::lexicographical_compare(lft->BannedIp.begin(),
												lft->BannedIp.end(), rght->BannedIp.begin(), rght->BannedIp.end());
		case SORT_BY_BANNER:
			return std::lexicographical_compare(lft->BannerName.begin(),
												lft->BannerName.end(),
												rght->BannerName.begin(), rght->BannerName.end());
		default: return true;
	}
}

///////////////////////////////////////////////////////////////////////////////
BanList::BanList() :
	current_sort_algorithm(0),
	current_proxy_sort_algorithm(0) {
	reload_ban();
	reload_proxy_ban();
	sort_ip(SORT_BY_NAME);
	sort_proxy(SORT_BY_NAME);
	purge_obsolete();
}

void BanList::sort_ip(int sort_algorithm) {
	if (sort_algorithm == current_sort_algorithm)
		return;

	Ban_List.sort([&](const BanNodePtr lhs, const BanNodePtr rhs) {
		return this->ban_sort_func(lhs, rhs, sort_algorithm);
	});

	current_sort_algorithm = sort_algorithm;
}

void BanList::sort_proxy(int sort_algorithm) {
	if (sort_algorithm == current_proxy_sort_algorithm)
		return;

	Proxy_Ban_List.sort([&](const ProxyBanNodePtr lhs, const ProxyBanNodePtr rhs) {
		return this->proxy_ban_sort_func(lhs, rhs, sort_algorithm);
	});

	current_proxy_sort_algorithm = sort_algorithm;
}

bool BanList::add_ban(std::string BannedIp, std::string BanReason, std::string BannerName, int UnbanDate, int BanType) {
	BanNodePtr temp_node_ptr(new struct BanNode);

	if (BannedIp.empty() || BannerName.empty())
		return false;

	// looking if the ip is already in the ban list
	std::list<BanNodePtr>::iterator i =
		std::find_if(Ban_List.begin(), Ban_List.end(), [&](const BanNodePtr ptr) {
			return this->ban_compare(ptr, BAN_IP_COMPARE, &BannedIp);
		});

	if (i != Ban_List.end())
		return false;

	temp_node_ptr->BannedIp = BannedIp;
	temp_node_ptr->BanReason = (BanReason.empty() ? "Unknown" : BanReason);
	temp_node_ptr->BannerName = BannerName;
	temp_node_ptr->BanDate = time(0);
	temp_node_ptr->UnbanDate = (UnbanDate > 0) ? time(0) + UnbanDate * 60 * 60 : BAN_MAX_TIME;
	temp_node_ptr->BanType = BanType;

	// checking all strings: replacing all whitespaces with underlines
	size_t k = temp_node_ptr->BannedIp.size();
	for (size_t j = 0; j < k; j++) {
		if (temp_node_ptr->BannedIp[j] == ' ') {
			temp_node_ptr->BannedIp[j] = '_';
		}
	}

	k = temp_node_ptr->BannerName.size();
	for (size_t j = 0; j < k; j++) {
		if (temp_node_ptr->BannerName[j] == ' ') {
			temp_node_ptr->BannerName[j] = '_';
		}
	}

	k = temp_node_ptr->BanReason.size();
	for (size_t j = 0; j < k; j++) {
		if (temp_node_ptr->BanReason[j] == ' ') {
			temp_node_ptr->BanReason[j] = '_';
		}
	}

	Ban_List.push_front(temp_node_ptr);
	current_sort_algorithm = SORT_UNDEFINED;
	save_ip();
///////////////////////////////////////////////////////////////////////
	if (BanType == 3)
		disconnectBannedIp(BannedIp);

	sprintf(buf, "%s has banned %s for %s players(%s) (%dh).",
			BannerName.c_str(), BannedIp.c_str(), ban_types[BanType], temp_node_ptr->BanReason.c_str(), UnbanDate);
	mudlog(buf, BRF, kLvlGod, SYSLOG, true);
	imm_log("%s has banned %s for %s players(%s) (%dh).", BannerName.c_str(),
			BannedIp.c_str(), ban_types[BanType], temp_node_ptr->BanReason.c_str(), UnbanDate);

///////////////////////////////////////////////////////////////////////
	return true;
}

bool BanList::add_proxy_ban(std::string BannedIp, std::string BannerName) {
	ProxyBanNodePtr temp_node_ptr(new struct ProxyBanNode);
	if (BannedIp.empty() || BannerName.empty())
		return false;

	std::list<ProxyBanNodePtr>::iterator i =
		std::find_if(Proxy_Ban_List.begin(), Proxy_Ban_List.end(), [&](const ProxyBanNodePtr ptr) {
			return this->proxy_ban_compare(ptr, BAN_IP_COMPARE, &BannedIp);
		});

	if (i != Proxy_Ban_List.end())
		return false;

	temp_node_ptr->BannedIp = BannedIp;
	temp_node_ptr->BannerName = BannerName;

	// checking all strings: replacing all whitespaces with underlines
	size_t k = temp_node_ptr->BannedIp.size();
	for (size_t j = 0; j < k; j++) {
		if (temp_node_ptr->BannedIp[j] == ' ') {
			temp_node_ptr->BannedIp[j] = '_';
		}
	}

	k = temp_node_ptr->BannerName.size();
	for (size_t j = 0; j < k; j++) {
		if (temp_node_ptr->BannerName[j] == ' ') {
			temp_node_ptr->BannerName[j] = '_';
		}
	}

	Proxy_Ban_List.push_front(temp_node_ptr);
	current_proxy_sort_algorithm = SORT_UNDEFINED;
	save_proxy();
///////////////////////////////////////////////////////////////////////
	disconnectBannedIp(BannedIp);
	sprintf(buf, "%s has banned proxy %s", BannerName.c_str(), BannedIp.c_str());
	mudlog(buf, BRF, kLvlGod, SYSLOG, true);
	imm_log("%s has banned proxy %s", BannerName.c_str(), BannedIp.c_str());
///////////////////////////////////////////////////////////////////////
	return true;
}

bool BanList::reload_ban(void) {
	FILE *loaded;
	Ban_List.clear();
	if ((loaded = fopen(ban_filename, "r"))) {
		std::string str_to_parse;

		while (!feof(loaded)) {
			DiskIo::read_line(loaded, str_to_parse, 1);
			std::vector<std::string> ban_tokens = utils::Split(str_to_parse);

			if (ban_tokens.size() != 6) {
				log("Ban list %s error, line %s", ban_filename, str_to_parse.c_str());
				continue;
			}
			BanNodePtr ptr(new struct BanNode);
			auto tok_iter = ban_tokens.begin();

			// type
			for (int i = BAN_NO; i <= BAN_ALL; i++) {
				if (!strcmp((*tok_iter).c_str(), ban_types[i])) {
					ptr->BanType = i;
				}
			} 
			// ip
			++tok_iter;
			ptr->BannedIp = (*tok_iter);
			// removing port specification i.e. 129.1.1.1:8080; :8080 is removed
			std::string::size_type at = ptr->BannedIp.find_first_of(":");
			if (at != std::string::npos) {
				ptr->BannedIp.erase(at);
			}
			//ban_date
			++tok_iter;
			ptr->BanDate = atol((*tok_iter).c_str());
			//banner_name
			++tok_iter;
			ptr->BannerName = (*tok_iter);
			//ban_length
			++tok_iter;
			ptr->UnbanDate = atol((*tok_iter).c_str());
			//ban_reason
			++tok_iter;
			ptr->BanReason = (*tok_iter);
			Ban_List.push_front(ptr);
		}
		fclose(loaded);
		return true;
	}
	log("SYSERR: Unable to open banfile");
	return false;
}

bool BanList::reload_proxy_ban() {
	FILE *loaded;

	loaded = fopen(proxy_ban_filename, "r");
	if (loaded) {
		std::string str_to_parse;

		while (DiskIo::read_line(loaded, str_to_parse, 1)) {
			std::vector<std::string> ban_tokens = utils::Split(str_to_parse);

			if (ban_tokens.size() != 2) {
				log("Proxy ban list %s error, line %s", proxy_ban_filename, str_to_parse.c_str());
				continue;
			}
			auto tok_iter = ban_tokens.begin();
			ProxyBanNodePtr ptr(new struct ProxyBanNode);

			ptr->BannerName = "Undefined";
			ptr->BannedIp = (*tok_iter);
			// removing port specification i.e. 129.1.1.1:8080; :8080 is removed
			std::string::size_type at = ptr->BannedIp.find_first_of(":");
			if (at != std::string::npos) {
				ptr->BannedIp.erase(at);
			} 
			++tok_iter;
			ptr->BannerName = (*tok_iter);
			Proxy_Ban_List.push_front(ptr);
			disconnectBannedIp(ptr->BannedIp);
		}
		fclose(loaded);
		return true;
	}
	log("SYSERR: Unable to open proxybanfile");
	return false;
}

bool BanList::save_ip(void) {
	FILE *loaded;
	if ((loaded = fopen(ban_filename, "w"))) {
		for (std::list<BanNodePtr>::iterator i = Ban_List.begin(); i != Ban_List.end(); i++) {
			fprintf(loaded, "%s %s %ld %s %ld %s\n",
					ban_types[(*i)->BanType], (*i)->BannedIp.c_str(),
					static_cast<long int>((*i)->BanDate), (*i)->BannerName.c_str(),
					static_cast<long int>((*i)->UnbanDate), (*i)->BanReason.c_str());
		}
		fclose(loaded);
		return true;
	}
	log("SYSERR: Unable to save banfile");
	return false;
}

bool BanList::save_proxy(void) {
	FILE *loaded;
	if ((loaded = fopen(proxy_ban_filename, "w"))) {
		for (std::list<ProxyBanNodePtr>::iterator i = Proxy_Ban_List.begin(); i != Proxy_Ban_List.end(); i++) {
			fprintf(loaded, "%s %s\n", (*i)->BannedIp.c_str(), (*i)->BannerName.c_str());
		}
		fclose(loaded);
		return true;
	}
	log("SYSERR: Unable to save proxybanfile");
	return false;
}

void BanList::ShowBannedIp(int sort_mode, CharData *ch) {
	if (Ban_List.empty()) {
		SendMsgToChar("No sites are banned.\r\n", ch);
		return;
	}

	sort_ip(sort_mode);
	char format[kMaxInputLength], to_unban[kMaxInputLength], buff[kMaxInputLength], *listbuf = 0, *timestr;
	strcpy(format, "%-25.25s  %-8.8s  %-10.10s  %-16.16s %-8.8s\r\n");
	sprintf(buf, format, "Banned Site Name", "Ban Type", "Banned On", "Banned By", "To Unban");

	listbuf = str_add(listbuf, buf);
	sprintf(buf, format,
			"---------------------------------",
			"---------------------------------",
			"---------------------------------",
			"---------------------------------", "---------------------------------");
	listbuf = str_add(listbuf, buf);

	for (std::list<BanNodePtr>::iterator i = Ban_List.begin(); i != Ban_List.end(); i++) {
		timestr = asctime(localtime(&((*i)->BanDate)));
		*(timestr + 10) = 0;
		strcpy(to_unban, timestr);
		sprintf(buff, "%ldh", static_cast<long int>((*i)->UnbanDate - time(nullptr)) / 3600);
		sprintf(buf, format, (*i)->BannedIp.c_str(), ban_types[(*i)->BanType],
				to_unban, (*i)->BannerName.c_str(), buff);
		listbuf = str_add(listbuf, buf);
		strcpy(buf, (*i)->BanReason.c_str());
		strcat(buf, "\r\n");
		listbuf = str_add(listbuf, buf);
	}
	page_string(ch->desc, listbuf, 1);
	free(listbuf);
}

void BanList::ShowBannedIpByMask(int sort_mode, CharData *ch, const char *mask) {
	bool is_find = false;
	if (Ban_List.empty()) {
		SendMsgToChar("No sites are banned.\r\n", ch);
		return;
	}

	sort_ip(sort_mode);
	char format[kMaxInputLength], to_unban[kMaxInputLength], buff[kMaxInputLength], *listbuf = 0, *timestr;
	strcpy(format, "%-25.25s  %-8.8s  %-10.10s  %-16.16s %-8.8s\r\n");
	sprintf(buf, format, "Banned Site Name", "Ban Type", "Banned On", "Banned By", "To Unban");

	listbuf = str_add(listbuf, buf);
	sprintf(buf, format,
			"---------------------------------",
			"---------------------------------",
			"---------------------------------",
			"---------------------------------", "---------------------------------");
	listbuf = str_add(listbuf, buf);

	for (std::list<BanNodePtr>::iterator i = Ban_List.begin(); i != Ban_List.end(); i++) {
		if (strncmp((*i)->BannedIp.c_str(), mask, strlen(mask)) == 0) {
			timestr = asctime(localtime(&((*i)->BanDate)));
			*(timestr + 10) = 0;
			strcpy(to_unban, timestr);
			sprintf(buff, "%ldh", static_cast<long int>((*i)->UnbanDate - time(nullptr)) / 3600);
			sprintf(buf, format, (*i)->BannedIp.c_str(), ban_types[(*i)->BanType],
					to_unban, (*i)->BannerName.c_str(), buff);
			listbuf = str_add(listbuf, buf);
			strcpy(buf, (*i)->BanReason.c_str());
			strcat(buf, "\r\n");
			listbuf = str_add(listbuf, buf);
			is_find = true;
		};

	}
	if (is_find) {
		page_string(ch->desc, listbuf, 1);
	} else {
		SendMsgToChar("No sites are banned.\r\n", ch);
	}
	free(listbuf);
}

void BanList::ShowBannedProxy(int sort_mode, CharData *ch) {
	if (Proxy_Ban_List.empty()) {
		SendMsgToChar("No proxies are banned.\r\n", ch);
		return;
	}
	sort_proxy(sort_mode);
	char format[kMaxInputLength], *listbuf = 0;
	strcpy(format, "%-25.25s  %-16.16s\r\n");
	sprintf(buf, format, "Banned Site Name", "Banned By");
	SendMsgToChar(buf, ch);
	sprintf(buf, format, "---------------------------------", "---------------------------------");
	listbuf = str_add(listbuf, buf);
	for (std::list<ProxyBanNodePtr>::iterator i = Proxy_Ban_List.begin(); i != Proxy_Ban_List.end(); i++) {
		sprintf(buf, format, (*i)->BannedIp.c_str(), (*i)->BannerName.c_str());
		listbuf = str_add(listbuf, buf);
	}
	page_string(ch->desc, listbuf, 1);
}

int BanList::is_banned(std::string ip) {
	std::list<ProxyBanNodePtr>::iterator i =
		std::find_if(Proxy_Ban_List.begin(), Proxy_Ban_List.end(), [&](const ProxyBanNodePtr ptr) {
			return this->proxy_ban_compare(ptr, BAN_IP_COMPARE, &ip);
		});

	if (i != Proxy_Ban_List.end())
		return BAN_ALL;

	std::list<BanNodePtr>::iterator j =
		std::find_if(Ban_List.begin(), Ban_List.end(), [&](const BanNodePtr ptr) {
			return this->ban_compare(ptr, BAN_IP_COMPARE, &ip);
		});

	if (j != Ban_List.end()) {
		if ((*j)->UnbanDate <= time(0)) {
			sprintf(buf, "Site %s is unbaned (time expired).", (*j)->BannedIp.c_str());
			mudlog(buf, NRM, kLvlGod, SYSLOG, true);
			Ban_List.erase(j);
			save_ip();
			return BAN_NO;
		}
		return (*j)->BanType;
	}
	return BAN_NO;
}

bool BanList::unban_ip(std::string ip, CharData *ch) {
	std::list<BanNodePtr>::iterator i =
		std::find_if(Ban_List.begin(), Ban_List.end(), [&](const BanNodePtr ptr) {
			return this->ban_compare(ptr, BAN_IP_COMPARE, &ip);
		});

	if (i != Ban_List.end()) {
////////////////////////////////////////////////////////////////////////
		SendMsgToChar("Site unbanned.\r\n", ch);
		sprintf(buf, "%s removed the %s-player ban on %s.",
				GET_NAME(ch), ban_types[(*i)->BanType], (*i)->BannedIp.c_str());
		mudlog(buf, BRF, std::max(kLvlGod, GET_INVIS_LEV(ch)), SYSLOG, true);
		imm_log("%s removed the %s-player ban on %s.", GET_NAME(ch),
				ban_types[(*i)->BanType], (*i)->BannedIp.c_str());
////////////////////////////////////////////////////////////////////////
		Ban_List.erase(i);
		save_ip();
		return true;
	}
	return false;
}

bool BanList::unban_proxy(std::string ip, CharData *ch) {
	std::list<ProxyBanNodePtr>::iterator i =
		std::find_if(Proxy_Ban_List.begin(), Proxy_Ban_List.end(), [&](const ProxyBanNodePtr ptr) {
			return this->proxy_ban_compare(ptr, BAN_IP_COMPARE, &ip);
		});

	if (i != Proxy_Ban_List.end()) {
////////////////////////////////////////////////////////////////////////
		SendMsgToChar("Proxy unbanned.\r\n", ch);
		sprintf(buf, "%s removed the proxy ban on %s.", GET_NAME(ch), (*i)->BannedIp.c_str());
		mudlog(buf, BRF, std::max(kLvlGod, GET_INVIS_LEV(ch)), SYSLOG, true);
		imm_log("%s removed the proxy ban on %s.", GET_NAME(ch), (*i)->BannedIp.c_str());
////////////////////////////////////////////////////////////////////////
		Proxy_Ban_List.erase(i);
		save_proxy();
		return true;
	}
	return false;
}

bool BanList::unban(std::string Ip, CharData *ch) {
	bool flag1 = false, flag2 = false;
	flag1 = unban_ip(Ip, ch);
	flag2 = unban_proxy(Ip, ch);
	if (flag1 || flag2)
		return true;
	return false;
}

bool BanList::empty_ip() {
	return Ban_List.empty();
}

bool BanList::empty_proxy() {
	return Proxy_Ban_List.empty();
}

void BanList::clear_all() {
	Ban_List.clear();
	Proxy_Ban_List.clear();
}

void BanList::purge_obsolete() {
	bool purged = false;
	for (std::list<BanNodePtr>::iterator i = Ban_List.begin(); i != Ban_List.end();) {
		if ((*i)->UnbanDate <= time(0)) {
			Ban_List.erase(i++);
			purged = true;
		} else
			i++;
	}
	if (purged)
		save_ip();
}

time_t BanList::getBanDate(std::string ip) {
	std::list<ProxyBanNodePtr>::iterator i =
		std::find_if(Proxy_Ban_List.begin(), Proxy_Ban_List.end(), [&](const ProxyBanNodePtr ptr) {
			return this->proxy_ban_compare(ptr, BAN_IP_COMPARE, &ip);
		});

	if (i != Proxy_Ban_List.end())
		return -1;    //infinite ban

	std::list<BanNodePtr>::iterator j =
		std::find_if(Ban_List.begin(), Ban_List.end(), [&](const BanNodePtr ptr) {
			return this->ban_compare(ptr, BAN_IP_COMPARE, &ip);
		});

	if (j != Ban_List.end()) {
		return (*j)->UnbanDate;
	}

	return time(0);

}

void BanList::disconnectBannedIp(std::string Ip) {
	DescriptorData *d;

	for (d = descriptor_list; d; d = d->next) {
		if (d->host == Ip) {
			if (STATE(d) == CON_DISCONNECT || STATE(d) == CON_CLOSE)
				return;
			//SendMsgToChar will crash, it char has not been loaded/created yet.
			SEND_TO_Q("Your IP has been banned, disconnecting...\r\n", d);
			if (STATE(d) == CON_PLAYING)
				STATE(d) = CON_DISCONNECT;
			else
				STATE(d) = CON_CLOSE;
		}
	}
	return;
}

//////////////////////////////////////////////////////////////////////////////////////

/**
* Система автоматической регистрации по ранее зарегистрированному мылу.
* Флаг регистрации выставляется только на время нахождения в игре и в случае
* снятия регистрации с персонажа - данное мыло удалется из списка, т.е. при
* следующих заходах автоматически уже не зарегистрирует никого с тем же мылом,
* но если они имели свои собсвенные регистрации через команду register, то
* их эта отмена в принципе не задевает, т.е. на работу статой системы регистраций
* в данном случае никакого влияния не оказываем и флаг EPlrFlag::REGISTERED не трогаем. -- Krodo
*/
namespace RegisterSystem {

typedef std::map<std::string, std::string> EmailListType;
// список зарегистрированных мыл
EmailListType email_list;
// файл для соъхранения/лоада
const char *REGISTERED_EMAIL_FILE = LIB_PLRSTUFF"registered-email.lst";
// т.к. список потенциально может быть довольно большим, то сейвить бум только в случае изменений в add и remove
bool need_save = 0;

} // namespace RegisterSystem

// * Добавления мыла в список + проставления флага EPlrFlag::REGISTERED, registered_email не выставляется
void RegisterSystem::add(CharData *ch, const char *text, const char *reason) {
	PLR_FLAGS(ch).set(EPlrFlag::kRegistred);
	if (!text || !reason) return;
	std::stringstream out;
	out << GET_NAME(ch) << " -> " << text << " [" << reason << "]";
	EmailListType::const_iterator it = email_list.find(GET_EMAIL(ch));
	if (it == email_list.end()) {
		email_list[GET_EMAIL(ch)] = out.str();
		need_save = 1;
	}
}

/**
* Удаление мыла из списка, снятие флага EPlrFlag::REGISTERED и registered_email.
* В течении секунды персонаж помещается в комнату незареганных игроков, если он не один с данного ип
*/
void RegisterSystem::remove(CharData *ch) {
	PLR_FLAGS(ch).unset(EPlrFlag::kRegistred);
	EmailListType::iterator it = email_list.find(GET_EMAIL(ch));
	if (it != email_list.end()) {
		email_list.erase(it);
		if (ch->desc)
			ch->desc->registered_email = 0;
		need_save = 1;
	}
}

/**
* Проверка, является ли персонаж зарегистрированным каким-нить образом
* \return 0 - нет, 1 - да
*/
bool RegisterSystem::is_registered(CharData *ch) {
	if (PLR_FLAGGED(ch, EPlrFlag::kRegistred) || (ch->desc && ch->desc->registered_email))
		return 1;
	return 0;
}

/**
* Поиск данного мыла в списке зарегистрированных
* \return 0 - не нашли, 1 - нашли
*/
bool RegisterSystem::is_registered_email(const std::string &email) {
	EmailListType::const_iterator it = email_list.find(email);
	if (it != email_list.end())
		return 1;
	return 0;
}

/**
* Показ информации по зарегистрированному мылу
* \return строка информации или пустая строка, если ничего не нашли
*/
const std::string RegisterSystem::show_comment(const std::string &email) {
	EmailListType::const_iterator it = email_list.find(email);
	if (it != email_list.end())
		return it->second;
	return "";
}

// * Загрузка и релоад списка мыл
void RegisterSystem::load() {
	email_list.clear();

	std::ifstream file(REGISTERED_EMAIL_FILE);
	if (!file.is_open()) {
		log("Error open file: %s! (%s %s %d)", REGISTERED_EMAIL_FILE, __FILE__, __func__, __LINE__);
		return;
	}
	std::string email, comment;
	while (file >> email) {
		ReadEndString(file);
		std::getline(file, comment);
		email_list[email] = comment;
	}
	file.close();
}

// * Сохранение списка мыл, если оно требуется
void RegisterSystem::save() {
	if (!need_save)
		return;
	std::ofstream file(REGISTERED_EMAIL_FILE);
	if (!file.is_open()) {
		log("Error open file: %s! (%s %s %d)", REGISTERED_EMAIL_FILE, __FILE__, __func__, __LINE__);
		return;
	}
	for (EmailListType::const_iterator it = email_list.begin(); it != email_list.end(); ++it)
		file << it->first << "\n" << it->second << "\n";
	file.close();
	need_save = 0;
}

// contains list of banned ip's and proxies + interface
BanList *&ban = GlobalObjects::ban();

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
