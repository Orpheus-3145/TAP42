#include <iostream>

#include "Exceptions.hpp"
#include "ArgParser.hpp"
#include "ThreadClient.hpp"


int32_t main(int32_t argc, char** argv)
{
	try {
		Flags options = parseArguments(argc, argv);
		if (options.helpmode == true) {
			std::cout << HOW_TO << std::endl;
			return (EXIT_SUCCESS);
		}

		ThreadClient client(
			[](std::string const& data)
			{
				// callback fissa, decisa una volta per tutte alla creazione
				std::cout << "[worker thread] dati ricevuti: " << data << "\n";
			},
			[](size_t n)
			{
				std::cout << "[worker thread] inviati " << n << " byte\n";
			},
			[](std::string const& what)
			{
				std::cerr << "[worker thread] errore: " << what << "\n";
			},
			[]
			{
				std::cout << "[worker thread] connessione chiusa\n";
			}
		);

		client.connect(options.host, options.port);
		client.send("halo porcoddio");

	} catch (AppException& err) {
		std::cerr << err.what() << std::endl;
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}