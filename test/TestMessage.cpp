// Nota: `#define private public` serve solo per leggere subType nei test.
// In alternativa aggiungi dei getter a Message (getErrorType(), getEventType()).

#include <iostream>
#include <string>
#include <cstdint>
#include <vector>

#include "Exceptions.hpp"

#define private public
#include "Message.hpp"
#undef private

static int g_total = 0;
static int g_failed = 0;

static void report(std::string const& name, bool ok, std::string const& detail = "")
{
	++g_total;
	if (ok)
	{
		std::cout << "[ OK ] " << name << "\n";
		return;
	}
	++g_failed;
	std::cout << "[FAIL] " << name;
	if (!detail.empty())
		std::cout << "  -> " << detail;
	std::cout << "\n";
}

static void group(std::string const& title)
{
	std::cout << "\n=== " << title << " ===\n";
}

// Il parsing deve riuscire e il messaggio deve essere una RESPONSE
static void expectResponse(std::string const& in)
{
	std::string name = "response  \"" + in + "\"";
	try
	{
		Message m(in);
		report(name,
			m.isResponse() && !m.isError() && !m.isEvent() && !m.isCommand(),
			"flag di tipo errati (response/error/event/command: "
			+ std::to_string(m.isResponse()) + std::to_string(m.isError())
			+ std::to_string(m.isEvent()) + std::to_string(m.isCommand()) + ")");
	}
	catch (std::exception const& e) { report(name, false, std::string("eccezione inattesa: ") + e.what()); }
	catch (...)                     { report(name, false, "eccezione inattesa"); }
}

// Il parsing deve riuscire, tipo ERROR e sottotipo corretto
static void expectError(std::string const& in, ErrorType expected)
{
	std::string name = "error     \"" + in + "\"";
	try
	{
		Message m(in);
		if (!(m.isError() && !m.isResponse() && !m.isEvent() && !m.isCommand()))
			report(name, false, "flag di tipo errati (isError deve essere l'unico true)");
		else
			report(name, m.subType.errType == expected,
				"errType = " + std::to_string(static_cast<unsigned>(m.subType.errType))
				+ ", atteso " + std::to_string(static_cast<unsigned>(expected)));
	}
	catch (std::exception const& e) { report(name, false, std::string("eccezione inattesa: ") + e.what()); }
	catch (...)                     { report(name, false, "eccezione inattesa"); }
}

// Il parsing deve riuscire, tipo EVENT e sottotipo corretto
static void expectEvent(std::string const& in, EventType expected)
{
	std::string name = "event     \"" + in + "\"";
	try
	{
		Message m(in);
		if (!(m.isEvent() && !m.isResponse() && !m.isError() && !m.isCommand()))
			report(name, false, "flag di tipo errati (isEvent deve essere l'unico true)");
		else
			report(name, m.subType.eventType == expected,
				"eventType = " + std::to_string(static_cast<unsigned>(m.subType.eventType))
				+ ", atteso " + std::to_string(static_cast<unsigned>(expected)));
	}
	catch (std::exception const& e) { report(name, false, std::string("eccezione inattesa: ") + e.what()); }
	catch (...)                     { report(name, false, "eccezione inattesa"); }
}

// Il parsing deve lanciare AppException
static void expectThrow(std::string const& in)
{
	std::string name = "throws    \"" + in + "\"";
	try
	{
		Message m(in);
		report(name, false, "nessuna eccezione lanciata");
	}
	catch (AppException const&) { report(name, true); }
	catch (...)                 { report(name, false, "eccezione di tipo diverso da AppException"); }
}

// Il parsing non deve lanciare (ignora il tipo)
static void expectNoThrow(std::string const& in)
{
	std::string name = "no-throw  \"" + in + "\"";
	try
	{
		Message m(in);
		report(name, true);
	}
	catch (std::exception const& e) { report(name, false, std::string("eccezione inattesa: ") + e.what()); }
	catch (...)                     { report(name, false, "eccezione inattesa"); }
}

int main()
{
	group("Messaggi non validi / generici");
	expectThrow("");
	expectThrow("   ");
	expectThrow("HELLO");
	expectThrow("HELLO world");
	expectThrow("ok");				// case sensitive
	expectThrow("OKAY");
	expectThrow("OK_connected");

	group("RESPONSE");
	expectResponse("OK");
	expectResponse("OK connected");
	expectResponse("OK bye");
	expectResponse("OK <talking>");
	expectResponse("OK players=3");
	expectResponse("OK {\"name\": \"fra\", \"hp\": 10}");
	expectResponse("OK {\"items\": [1, 2, 3]}");
	expectThrow("OK {not valid json");

	group("ERROR");
	expectError("ERR 201 NAME_IN_USE", ErrorType::NAME_IN_USE);
	expectError("ERR 301 NO_EXIT", ErrorType::NO_EXIT);
	expectError("ERR 401 NOT_IN_GROUP", ErrorType::NOT_IN_GROUP);
	expectError("ERR 402 ALREADY_IN_GROUP", ErrorType::ALREADY_IN_GROUP);
	expectError("ERR 404 ITEM_NOT_FOUND", ErrorType::NOT_FOUND);
	expectError("ERR 405 NPC_NOT_HOSTILE", ErrorType::NPC_NOT_HOSTILE);
	expectError("ERR 406 NO_QUEST_AVAILABLE", ErrorType::NO_QUEST_AVAILABLE);
	expectError("ERR 900 CONNECTION_FAILED", ErrorType::CONNECTION_FAILED);
	expectError("ERR 901 SEND_FAILED", ErrorType::SEND_FAILED);
	expectThrow("ERR");
	expectThrow("ERR ");
	expectThrow("ERR abc NAME_IN_USE");		// codice non numerico
	expectThrow("ERR NAME_IN_USE");
	expectThrow("ERR 201");					// manca la descrizione

	group("EVENT");
	expectEvent("EVT ROOM PRESENCE_ENTER id=5", EventType::PRESENCE_ENTER);
	expectEvent("EVT ROOM PRESENCE_LEAVE id=5", EventType::PRESENCE_LEAVE);
	expectEvent("EVT ROOM CHAT id=5", EventType::ROOM_CHAT);
	expectEvent("EVT GLOBAL CHAT id=5", EventType::GLOBAL_CHAT);
	expectEvent("EVT GROUP JOIN id=5", EventType::GROUP_JOIN);
	expectEvent("EVT GROUP LEAVE id=5", EventType::GROUP_LEAVE);
	expectEvent("EVT GROUP INVITE id=5", EventType::GROUP_INVITE);
	expectEvent("EVT STATS players=3", EventType::STATS);
	expectEvent("EVT ROOM ITEM_USE id=5", EventType::ITEM_USE);
	expectEvent("EVT ROOM COMBAT damage=7", EventType::COMBAT);
	expectEvent("EVT ROOM COMBAT_DEATH_NPC id=5", EventType::COMBAT_DEATH_NPC);
	expectEvent("EVT ROOM COMBAT_DEATH_PLAYER id=5", EventType::COMBAT_DEATH_PLAYER);

	group("EVENT non validi");
	expectThrow("EVT");
	expectThrow("EVT ");
	expectThrow("EVT ROOM");
	expectThrow("EVT ROOM CHAT");				// manca il data
	expectThrow("EVT FOO BAR id=5");			// visibility sconosciuta
	expectThrow("EVT ROOM UNKNOWN id=5");		// nome sconosciuto
	expectThrow("EVT GLOBAL PRESENCE_ENTER id=5");	// combinazione non mappata
	expectThrow("EVT ROOM CHAT id=");			// manca il valore
	expectThrow("EVT ROOM CHAT =5");			// manca la chiave
	expectThrow("EVT STATS");

	group("Comportamenti attesi (probabili bug nel codice attuale)");
	expectEvent("EVT GROUP CHAT id=5", EventType::GROUP_CHAT);	// getEvent ritorna ROOM_CHAT
	expectNoThrow("EVT ROOM CHAT msg=hello");					// value non numerico -> json invalido
	expectNoThrow("EVT ROOM CHAT msg=hello world");				// data = solo la prima parola

	std::cout << "\n" << (g_total - g_failed) << "/" << g_total << " test passati\n";
	return g_failed == 0 ? 0 : 1;
}