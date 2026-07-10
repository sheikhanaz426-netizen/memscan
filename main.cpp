#include "memscan.hpp"
#include <iostream>

static ValType askType() {
    std::cout << "type? 1=byte (flags)  2=int (cash)  3=float (nitrous/speed): ";
    int t; std::cin >> t;
    if (t == 1) return ValType::Byte;
    if (t == 3) return ValType::Float;
    return ValType::Int;
}

static std::vector<Hit> askAddresses() {
    std::cout << "how many addresses? ";
    int n; std::cin >> n;
    std::vector<Hit> v;
    for (int i = 0; i < n; i++) {
        std::cout << "  #" << (i + 1) << " (hex): ";
        std::string s; std::cin >> s;
        v.push_back({ (uintptr_t)std::stoull(s, nullptr, 16), 0 });
    }
    return v;
}

static void printResults(const std::vector<Hit>& hits) {
    size_t shown = 0;
    for (auto& h : hits) {
        std::cout << "  0x" << std::hex << h.address << std::dec << " = " << h.value << "\n";
        if (++shown >= 30) { std::cout << "  ...(first 30)\n"; break; }
    }
}

int main() {
    std::cout << "memscan - a from-scratch memory scanner\n";
    std::cout << "for learning reverse engineering on offline games you own.\n\n";

    std::cout << "process name (e.g. NFS13.exe): ";
    std::string name; std::getline(std::cin, name);

    MemScanner scanner;
    if (!scanner.attach(name)) {
        std::cout << "could not attach. is the game running? run this as admin.\n";
        return 1;
    }
    std::cout << "attached.\n";

    while (true) {
        std::cout << "\n[" << scanner.results().size() << " candidates]\n"
                     "  1) exact scan        2) unknown scan (snapshot)\n"
                     "  3) changed  4) unchanged  5) increased  6) decreased\n"
                     "  7) list   8) write   9) freeze   10) stop freeze   0) quit\n> ";
        int c;
        if (!(std::cin >> c)) break;

        switch (c) {
            case 1: {
                ValType t = askType();
                std::cout << "value: "; double v; std::cin >> v;
                scanner.exactScan(t, v);
                std::cout << scanner.results().size() << " matches.\n";
                break;
            }
            case 2: {
                ValType t = askType();
                std::cout << "snapshotting... change the value in-game, then filter.\n";
                scanner.snapshot(t);
                break;
            }
            case 3: scanner.filter(Filter::Changed);   std::cout << scanner.results().size() << " left.\n"; break;
            case 4: scanner.filter(Filter::Unchanged); std::cout << scanner.results().size() << " left.\n"; break;
            case 5: scanner.filter(Filter::Increased); std::cout << scanner.results().size() << " left.\n"; break;
            case 6: scanner.filter(Filter::Decreased); std::cout << scanner.results().size() << " left.\n"; break;
            case 7: printResults(scanner.results()); break;
            case 8: {
                auto addrs = askAddresses();
                ValType t = askType();
                std::cout << "value to write to all: "; double v; std::cin >> v;
                for (auto& a : addrs)
                    std::cout << "  0x" << std::hex << a.address << std::dec
                              << (scanner.write(a.address, v, t) ? " written\n" : " FAILED\n");
                break;
            }
            case 9: {
                auto addrs = askAddresses();
                ValType t = askType();
                std::cout << "freeze all at: "; double v; std::cin >> v;
                for (auto& a : addrs) a.value = v;
                scanner.freeze(addrs, t);
                std::cout << "freezing " << addrs.size() << ".\n";
                break;
            }
            case 10: scanner.stopFreeze(); std::cout << "freezes cleared.\n"; break;
            case 0:  return 0;
        }
    }
    return 0;
}
