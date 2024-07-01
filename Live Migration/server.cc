#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <bitset>
#include <vector>
#include <iomanip>
#include <cstdint>
#include <regex>
#include <unistd.h>
#include <asio.hpp>

using namespace std;
using RegistersMap = std::map<std::string, int32_t>;
using asio::ip::tcp;
using std::vector;

class VirtualMachine {
	public:
	    VirtualMachine();
	    void configureVirtualMachine(int execSliceInInstructions);
	    void readAssemblyInstructions(const string& filePath);
	    void executeAssemblyInstructions(const string& virtualMachineName);
	    void dumpProcessorState(const string& virtualMachineName);
        void setRegisters(const RegistersMap& new_registers);
        
        pair<RegistersMap, int> deserialize(const std::vector<char>& buffer);
        
	    int programCounter;
	    vector<string> instructions;
	
	private:
	    void executeAssemblyInstruction(const string& instruction, const string& virtualMachineName);
	
	    int virtualMachineExecSliceInInstructions;
	    map<uint32_t, int32_t> memory;
	    map<string, int32_t> registers;
};

VirtualMachine::VirtualMachine(): programCounter(0), virtualMachineExecSliceInInstructions(0) {
  registers["$R0"] = 0;
  
  for (int i = 0; i < 32; ++i) {
    string regName = "$" + to_string(i);
    registers[regName] = 0;
  }
}

void VirtualMachine::setRegisters(const RegistersMap& new_registers) {
    registers = new_registers;
}

pair<RegistersMap, int> deserialize(const std::vector<char>& buffer) {
    int programCounter;
    RegistersMap registers;

    size_t pos = 0;
    memcpy(&programCounter, buffer.data() + pos, sizeof(programCounter));
    pos += sizeof(programCounter);

    while (pos < buffer.size()) {
        int32_t keySize;
        memcpy(&keySize, buffer.data() + pos, sizeof(keySize));
        pos += sizeof(keySize);

        std::string key(buffer.begin() + pos, buffer.begin() + pos + keySize);
        pos += keySize;

        int32_t value;
        memcpy(&value, buffer.data() + pos, sizeof(value));
        pos += sizeof(value);

        registers[key] = value;
    }

    return std::make_pair(registers, programCounter);
}

void VirtualMachine::configureVirtualMachine(int execSliceInInstructions) {
    this->virtualMachineExecSliceInInstructions = execSliceInInstructions;
}

void VirtualMachine::readAssemblyInstructions(const string& filePath) {
    ifstream infile(filePath);
    if (!infile.is_open()) {
        cerr << "Error while opening file " << filePath << endl;
        return;
    }

    string ln;
    while (getline(infile, ln)) {
        instructions.push_back(ln);
    }
}

void VirtualMachine::executeAssemblyInstructions(const string& virtualMachineName) {
    int counter = 0;
    
    while (programCounter < instructions.size() && counter < virtualMachineExecSliceInInstructions) {
        executeAssemblyInstruction(instructions[programCounter], virtualMachineName);
        counter++;
		programCounter++;
    }
}

void VirtualMachine::executeAssemblyInstruction(const string& assemblyInstruction, const string& virtualMachineName) {
	regex opCodeRegex("([a-z]+)");
    smatch opCodeMatch;
    regex_search(assemblyInstruction, opCodeMatch, opCodeRegex);
    string opcode = opCodeMatch.str(1);
    
    if (opcode == "li") {
        regex liRegex("li\\s+(\\$\\d+)\\s*,\\s*(-?\\d+)");
        smatch liMatch;
        regex_search(assemblyInstruction, liMatch, liRegex);

        string reg = liMatch.str(1);
        int32_t immediate = stoi(liMatch.str(2));
        registers[reg] = immediate;
    } else if (opcode == "add") {
        regex addRegex("add\\s+(\\$\\d+),\\s*(\\$\\d+),\\s*(\\$\\d+)");
        smatch addMatch;
        regex_search(assemblyInstruction, addMatch, addRegex);

        string rd = addMatch.str(1);
        string rs = addMatch.str(2);
        string rt = addMatch.str(3);
        registers[rd] = registers[rs] + registers[rt];
    } else if (opcode == "addi") {
        regex addiRegex("addi\\s+(\\$\\d+),\\s*(\\$\\d+),\\s*(-?\\d+)");
        smatch addiMatch;
        regex_search(assemblyInstruction, addiMatch, addiRegex);

        string rt = addiMatch.str(1);
        string rs = addiMatch.str(2);
        int32_t immediate = stoi(addiMatch.str(3));
        registers[rt] = registers[rs] + immediate;
    } else if (opcode == "sub") {
        regex subRegex("sub\\s+(\\$\\d+),\\s*(\\$\\d+),\\s*(\\$\\d+)");
        smatch subMatch;
        regex_search(assemblyInstruction, subMatch, subRegex);

        string rd = subMatch.str(1);
        string rs = subMatch.str(2);
        string rt = subMatch.str(3);
        registers[rd] = registers[rs] - registers[rt];
    } else if (opcode == "mul") {
        regex mulRegex("mul\\s+(\\$\\d+),\\s*(\\$\\d+),\\s*(\\$\\d+)");
        smatch mulMatch;
        regex_search(assemblyInstruction, mulMatch, mulRegex);

        string rd = mulMatch.str(1);
        string rs = mulMatch.str(2);
        string rt = mulMatch.str(3);
        registers[rd] = registers[rs] * registers[rt];
    } else if (opcode == "and") {
        regex andRegex("and\\s+(\\$\\d+),\\s*(\\$\\d+),\\s*(\\$\\d+)");
        smatch andMatch;
        regex_search(assemblyInstruction, andMatch, andRegex);

        string rd = andMatch.str(1);
        string rs = andMatch.str(2);
        string rt = andMatch.str(3);
        registers[rd] = registers[rs] & registers[rt];
    } else if (opcode == "or") {
        regex orRegex("or\\s+(\\$\\d+),\\s*(\\$\\d+)(?:,\\s*(\\$\\d+)|,\\s*(-?\\d+))");
	    smatch orMatch;
	    regex_search(assemblyInstruction, orMatch, orRegex);
	
	    string rd = orMatch.str(1);
	    string rs = orMatch.str(2);
	
	    if (orMatch[3].matched) {
	        string rt = orMatch.str(3);
	        registers[rd] = registers[rs] | registers[rt];
	    } else if (orMatch[4].matched) {
	        int32_t immediate = stoi(orMatch.str(4));
	        registers[rd] = registers[rs] | immediate;
	    }
    } else if (opcode == "xor") {
        regex xorRegex("xor\\s+(\\$\\d+),\\s*(\\$\\d+),\\s*(\\$\\d+)");
        smatch xorMatch;
        regex_search(assemblyInstruction, xorMatch, xorRegex);

        string rd = xorMatch.str(1);
        string rs = xorMatch.str(2);
        string rt = xorMatch.str(3);
        registers[rd] = registers[rs] ^ registers[rt];
    } else if (opcode == "sll") {
        regex sllRegex("sll\\s+(\\$\\d+),\\s*(\\$\\d+),\\s*(\\d+)");
        smatch sllMatch;
        regex_search(assemblyInstruction, sllMatch, sllRegex);

        string rd = sllMatch.str(1);
        string rt = sllMatch.str(2);
        int32_t shiftAmount = stoi(sllMatch.str(3));
        registers[rd] = registers[rt] << shiftAmount;
    } else if (opcode == "srl") {
        regex srlRegex("srl\\s+(\\$\\d+),\\s*(\\$\\d+),\\s*(\\d+)");
        smatch srlMatch;
        regex_search(assemblyInstruction, srlMatch, srlRegex);

        string rd = srlMatch.str(1);
        string rt = srlMatch.str(2);
        int32_t shiftAmount = stoi(srlMatch.str(3));
        registers[rd] = registers[rt] >> shiftAmount;
    } else if (opcode == "DUMP_PROCESSOR_STATE") {
        dumpProcessorState(virtualMachineName);
    }
}
        
void VirtualMachine::dumpProcessorState(const string& virtualMachineName) {
    cout << endl << "Register values for " + virtualMachineName << endl << endl;
    
	for (int i = 1; i <= 31; ++i) {
        string reg = "$" + to_string(i);
        cout << "R" << i << ": " << registers[reg] << endl;
    }
}

int main(int argc, char *argv[]) {
	string assembly_file_vm_1;

    int option;
    
    while ((option = getopt(argc, argv, "v:")) != -1) {
        switch (option) {
            case 'v':
                if (assembly_file_vm_1.empty()) {
                    assembly_file_vm_1 = optarg;
                } else {
                    cerr << "Only one input file allowed" << endl;
                    return 1;
                }
                break;
            default:
                cerr << "Use " << argv[0] << " -v assembly_file_vm_1" << endl;
                return 1;
        }
    }

    if (assembly_file_vm_1.empty()) {
        cerr << "Input Assembly File" << endl;
        cerr << "Use " << argv[0] << " -v assembly_file_vm_1" << endl;
        return 1;
    }

    VirtualMachine virtual_machine_1;
    int virtual_machine_1_exec_slice_in_instructions = 0;
    string virtual_machine_1_binary;
    
    asio::io_context io_context;
    
	try {
        cout << "Server is Running" << endl;
        
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8080));

        for (;;) {

            tcp::socket socket(io_context);
            acceptor.accept(socket);

            uint32_t dataSize;
            asio::read(socket, asio::buffer(&dataSize, sizeof(dataSize)));
            dataSize = ntohl(dataSize);

            std::vector<char> serializedData(dataSize);
            asio::read(socket, asio::buffer(serializedData));

            std::pair<std::map<std::string, int32_t>, int> receivedData = deserialize(serializedData);

            virtual_machine_1.setRegisters(receivedData.first);
            virtual_machine_1.programCounter = receivedData.second + 1;
            
            ifstream config1(assembly_file_vm_1);
            if (!config1.is_open()) {
                cerr << "Error opening configuration files" << endl;
                return 1;
            }

            string line;
            while (getline(config1, line)) {
                if (line.find("vm_exec_slice_in_instructions=") != string::npos) {
                    virtual_machine_1_exec_slice_in_instructions = stoi(line.substr(line.find("=") + 1));
                } else if (line.find("vm_binary=") != string::npos) {
                    virtual_machine_1_binary = line.substr(line.find("=") + 1);
                }
            }

            virtual_machine_1.configureVirtualMachine(virtual_machine_1_exec_slice_in_instructions);
            virtual_machine_1.readAssemblyInstructions(virtual_machine_1_binary);
            
            cout << endl << "After migrate to remote server program counter value is " << virtual_machine_1.programCounter << endl;

            while (virtual_machine_1.programCounter < virtual_machine_1.instructions.size()) {
                if (virtual_machine_1.programCounter < virtual_machine_1.instructions.size()) {
                    virtual_machine_1.executeAssemblyInstructions("Remote Machine");
                }
            }

            cout << endl << "Dump Processor State" << endl;

            virtual_machine_1.dumpProcessorState("Remote Machine");

            cout << endl;

            return 0;
        }
    } catch (std::exception& e) {
        std::cerr << "Exception in listenForData: " << e.what() << std::endl;
    }
}