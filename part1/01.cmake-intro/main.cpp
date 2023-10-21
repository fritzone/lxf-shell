#include <iostream>
#include <cstdlib>

int main()
{
	while(true)
	{
		std::cout << "lxfsh$ ";
		std::string command;
		std::getline(std::cin, command);
		if(command == "exit")
		{
			exit(0);
		}
		system(command.c_str());
	}
}