#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <stdexcept>
#include <limits>
using namespace std;

// ===================== БЛЮДО =====================
class bludo {
    string name; double price; int time;
public:
    bludo(string n, double p, int t) : name(n), price(p), time(t) {}
    string getn() const { return name; }
    double getp() const { return price; }
    int gett() const { return time; }
    void info() const { cout << "🍩 " << name << " | " << price << " руб. | " << time << " мин\n"; }
};

// ===================== ЗАКАЗ =====================
class order {
    static int nextid; int id; vector<bludo> items; string status;
public:
    order(vector<bludo> it) : id(nextid++), items(it), status("принят") {}
    double total() const { double sum = 0; for (auto& i : items) sum += i.getp(); return sum; }
    void setstat(string s) { status = s; }
    void info() const { cout << "🧾 заказ #" << id << " (" << status << ") | сумма: " << total() << " руб.\n"; for (auto& i : items) i.info(); }
    int getid() const { return id; }
};
int order::nextid = 1000;

// ===================== ГОСТЬ =====================
class guest {
    string name;
public:
    guest(string n) : name(n) {}
    void makeorder(vector<bludo> menu) {
        static random_device rd; static mt19937 gen(rd()); uniform_int_distribution<> dis(0, menu.size()-1);
        vector<bludo> items; items.push_back(menu[dis(gen)]);
        order o(items);
        cout << "🍽️ гость " << name << " заказал на " << o.total() << " руб.\n";
    }
};

// ===================== ОТЗЫВ (5 типов) =====================
enum class review_type { EXCELLENT, GOOD, NEUTRAL, BAD, UNACCEPTABLE };
class review {
protected: string text; review_type type; int score;
public:
    review(string t, review_type tp) : text(t), type(tp) {
        if (type == review_type::EXCELLENT) score = 5;
        else if (type == review_type::GOOD) score = 4;
        else if (type == review_type::NEUTRAL) score = 3;
        else if (type == review_type::BAD) score = 2;
        else score = 1;
    }
    virtual ~review() {}
    virtual void process(class shop& s, class barista* b) = 0;
    int getscore() const { return score; }
    review_type gettype() const { return type; }
};

class review_shop : public review {
public:
    review_shop(string t, review_type tp) : review(t, tp) {}
    void process(shop& s, barista* b) override;
};

class review_barista : public review {
    string baristaname;
public:
    review_barista(string t, review_type tp, string bn) : review(t, tp), baristaname(bn) {}
    void process(shop& s, barista* b) override;
};

// ===================== ЧЕЛОВЕК =====================
class person {
protected: string name; double salary;
public:
    person(string n, double s) : name(n), salary(s) {}
    virtual ~person() {}
    virtual void work() const { cout << name << " работает.\n"; }
    virtual void info() const { cout << "сотрудник: " << name << ", зп: " << salary << endl; }
    string getname() const { return name; }
    double getsalary() const { return salary; }
    void setsalary(double s) { salary = s; }
};

// ===================== БАРИСТА =====================
class barista : public person {
    int perf; int* rating; static int countb; bool warned;
public:
    barista() : person("ноунейм", 30000), perf(20), rating(new int(5)), warned(false) { countb++; }
    barista(string n, double s, int p) : person(n, s), perf(p), rating(new int(5)), warned(false) { countb++; if (p <= 0) throw runtime_error("ошибка: производительность > 0"); }
    barista(const barista& other) : person(other.name, other.salary), perf(other.perf), rating(new int(*other.rating)), warned(other.warned) { countb++; }
    ~barista() { delete rating; countb--; }
    void info() const override { cout << "👨‍🍳 " << name << " | зп: " << salary << " | произв: " << perf << " | рейтинг: " << *rating << "⭐\n"; }
    void work() const override { cout << "☕ " << name << " готовит кофе (" << perf << " напитков/час)\n"; }
    barista& operator++() { perf++; cout << "📈 производительность " << name << " → " << perf << endl; return *this; }
    barista operator++(int) { barista temp = *this; perf++; return temp; }
    static int getcountb() { return countb; }
    int getperf() const { return perf; }
    void changerat(int r) { if (rating) *rating = r; }
    int getrat() const { return rating ? *rating : 5; }
    bool iswarned() const { return warned; }
    void setwarned(bool w) { warned = w; }
};
int barista::countb = 0;

// ===================== КОФЕЙНЯ =====================
class shop {
    string name; double money; int gflow; int openh; vector<barista> staff; static int counts; vector<bludo> menu;
public:
    shop() : name("ноунейм"), money(0), gflow(0), openh(12) { counts++; }
    shop(string n, int flow, int hours) : name(n), money(0), gflow(flow), openh(hours) { counts++; if (flow < 0 || hours <= 0) throw invalid_argument("ошибка параметров"); }
    ~shop() { counts--; }
    void addb(barista b) { staff.push_back(b); cout << "✅ " << b.getname() << " нанят в " << name << "\n"; }
    void delb(string n) { for (auto it = staff.begin(); it != staff.end(); ++it) if (it->getname() == n) { cout << "❌ " << n << " уволен\n"; staff.erase(it); break; } }
    void addmenu(bludo b) { menu.push_back(b); }
    vector<bludo>& getmenu() { return menu; }
    double sumdmoney() { if (staff.empty()) return 0; int sumperf = 0; for (auto& b : staff) sumperf += b.getperf(); int maxdr = sumperf * openh; int served = min(gflow, maxdr); double rev = served * 300; random_device rd; mt19937 gen(rd()); uniform_real_distribution<> dis(0.8, 1.2); return rev * dis(gen); }
    double sumdsalary() { double total = 0; for (auto& b : staff) total += b.getsalary() / 30; return total; }
    void addprofit(double p) { money += p; }
    double getprofit() const { return money; }
    void applyreviewshop(review_type tp) {
        if (tp == review_type::EXCELLENT) { gflow = gflow * 1.15; cout << "⭐ поток вырос на 15% (теперь " << gflow << ")\n"; }
        else if (tp == review_type::GOOD) { gflow = gflow * 1.05; cout << "👍 поток вырос на 5% (теперь " << gflow << ")\n"; }
        else if (tp == review_type::BAD) { gflow = gflow * 0.9; cout << "👎 поток упал на 10% (теперь " << gflow << ")\n"; }
        else if (tp == review_type::UNACCEPTABLE) { gflow = gflow * 0.8; cout << "💀 поток упал на 20% (теперь " << gflow << ")\n"; }
        if (gflow < 10) gflow = 10;
    }
    void applyreviewbarista(barista& b, review_type tp) {
        if (tp == review_type::EXCELLENT) { b.setsalary(b.getsalary() * 1.1); cout << "🌟 " << b.getname() << " +10% зп (теперь " << b.getsalary() << ")\n"; b.changerat(5); }
        else if (tp == review_type::GOOD) { b.setsalary(b.getsalary() * 1.05); cout << "👍 " << b.getname() << " +5% зп (теперь " << b.getsalary() << ")\n"; b.changerat(4); }
        else if (tp == review_type::NEUTRAL) { cout << "😐 " << b.getname() << " без изменений\n"; b.changerat(3); }
        else if (tp == review_type::BAD) { b.setwarned(true); cout << "⚠️ " << b.getname() << " получил предупреждение\n"; b.changerat(2); }
        else if (tp == review_type::UNACCEPTABLE) { cout << "💀 " << b.getname() << " УВОЛЕН!\n"; delb(b.getname()); }
    }
    void livesh_realtime(int seconds, double& network_budget) {
        cout << "\n🌞 РЕАЛЬНЫЙ ДЕНЬ В " << name << " (" << seconds << " сек)\n";
        if (staff.empty()) { cout << "❌ нет бариста → день пропущен\n"; return; }
        auto start = chrono::steady_clock::now();
        random_device rd; mt19937 gen(rd()); uniform_int_distribution<> eventdist(0, 2);
        int ordersdone = 0;
        while (chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - start).count() < seconds) {
            this_thread::sleep_for(chrono::milliseconds(500));
            int r = eventdist(gen);
            if (r == 0 && gflow > 0 && menu.size() > 0) {
                static int guestid = 1; guest g("гость_" + to_string(guestid++));
                g.makeorder(menu); ordersdone++;
                double rev = 150 + (rand() % 200); money += rev; cout << "💰 +" << rev << " руб. (выручка " << (int)money << ")\n";
            }
        }
        double salary_cost = sumdsalary(); money -= salary_cost; network_budget += (money - 0);
        cout << "\n📊 КОНЕЦ ДНЯ: заказов " << ordersdone << ", зарплаты -" << (int)salary_cost << ", выручка " << (int)money << "\n";
        random_device rd2; mt19937 gen2(rd2());
        int rshop = gen2() % 5; review_type shoptp = (review_type)rshop;
        review_shop rshopobj("отзыв о кофейне", shoptp); rshopobj.process(*this, nullptr);
        for (auto& b : staff) {
            int rbar = gen2() % 5; review_type bartp = (review_type)rbar;
            review_barista rbarobj("отзыв о " + b.getname(), bartp, b.getname()); rbarobj.process(*this, &b);
        }
        cout << "📈 поток гостей на завтра: " << gflow << "\n";
    }
    void livesh_fast(double& network_budget) {
        double rev = sumdmoney(); double sal = sumdsalary(); double profit = rev - sal; money += profit; network_budget += profit;
        cout << "📆 " << name << " | прибыль: " << (int)profit << " | выручка: " << (int)money << "\n";
        random_device rd; mt19937 gen(rd()); uniform_int_distribution<> flowChange(-5, 10);
        gflow += flowChange(gen); if (gflow < 10) gflow = 10;
    }
    int getmaxdr() const { int total = 0; for (auto& b : staff) total += b.getperf(); return total * openh; }
    friend ostream& operator<<(ostream& os, const shop& s);
    shop& operator++() { gflow += 5; cout << "📈 поток в " << name << " → " << gflow << endl; return *this; }
    shop operator++(int) { shop now = *this; gflow += 5; return now; }
    void info() { cout << "🏪 " << name << " | выручка: " << (int)money << " | гостей: " << gflow << " | персонал: " << staff.size() << endl; }
    void info(bool flag) { if (!flag) { info(); return; }
        cout << "\n╔══════════════════════════╗\n║ 🏪 " << name << "\n║ 💰 " << (int)money << " руб.\n║ 👥 " << gflow << "\n║ 👨‍🍳 " << staff.size() << "\n╚══════════════════════════╝\n"; }
    string getn() const { return name; }
    double getm() const { return money; }
    int getflow() const { return gflow; }
    vector<barista>& getstaff() { return staff; }
    static int getcounts() { return counts; }
};
ostream& operator<<(ostream& os, const shop& s) { os << s.name << " | 💰" << (int)s.money << " | 👥" << s.gflow; return os; }
int shop::counts = 0;

void review_shop::process(shop& s, barista*) { s.applyreviewshop(gettype()); }
void review_barista::process(shop& s, barista* b) { if (b) s.applyreviewbarista(*b, gettype()); }

// ===================== СЕТЬ С ЭКОНОМИКОЙ =====================
class net {
    string name; double budget; vector<shop> shops; static const double SHOP_COST;
public:
    net(string n, double b) : name(n), budget(b) { cout << "💰 СТАРТОВЫЙ КАПИТАЛ: " << budget << " руб.\n"; }
    bool can_afford_shop() const { return budget >= SHOP_COST; }
    void buy_shop(shop s) {
        if (!can_afford_shop()) { cout << "❌ не хватает денег! Нужно " << SHOP_COST << ", есть " << budget << "\n"; return; }
        budget -= SHOP_COST; shops.push_back(s); cout << "🏢 куплена " << s.getn() << " ( -" << SHOP_COST << " руб. )\n";
    }
    void liverealtime(int days, int secondsperday) {
        for (int i = 0; i < days; i++) {
            if (budget < 0) { cout << "\n💀 БАНКРОТСТВО! Бюджет сети: " << budget << " руб. Игра завершена.\n"; exit(0); }
            cout << "\n🔥 ДЕНЬ " << i+1 << " | Бюджет сети: " << budget << " руб. 🔥\n";
            for (auto& s : shops) s.livesh_realtime(secondsperday, budget);
            if (i < days-1) { cout << "\n⏳ 2 сек до следующего дня...\n"; this_thread::sleep_for(chrono::seconds(2)); }
        }
        cout << "\n💰 КОНЕЦ СИМУЛЯЦИИ. Бюджет сети: " << budget << " руб.\n";
    }
    void livefast(int days) {
        for (int i = 0; i < days; i++) {
            if (budget < 0) { cout << "\n💀 БАНКРОТСТВО! Бюджет сети: " << budget << " руб. Игра завершена.\n"; exit(0); }
            cout << "📊 день " << i+1 << " | бюджет: " << budget << " руб.\n";
            for (auto& s : shops) s.livesh_fast(budget);
        }
        cout << "\n💰 БЮДЖЕТ СЕТИ ПОСЛЕ " << days << " ДНЕЙ: " << budget << " руб.\n";
    }
    void showall() { if (shops.empty()) { cout << "❌ нет кофеен\n"; return; }
        cout << "\n══════════ СЕТЬ: " << name << " (бюджет: " << budget << ") ══════════\n";
        for (size_t i = 0; i < shops.size(); i++) cout << i+1 << ". " << shops[i] << endl;
    }
    void cheking() { cout << "\n📊 АНАЛИЗ СЕТИ " << name << "\n";
        for (auto& s : shops) { cout << "\n--- " << s.getn() << " ---\n";
            if (s.getflow() > s.getmaxdr()) cout << "⚠️ нужен ещё бариста\n";
            else if (s.getm() < 10000 && s.getflow() < 30) cout << "🔴 рекомендуется закрыть\n";
            else cout << "✅ стабильно\n";
        }
    }
    shop* getshop(int idx) { if (idx >= 1 && idx <= (int)shops.size()) return &shops[idx-1]; return nullptr; }
    double getbudget() const { return budget; }
};
const double net::SHOP_COST = 300000;

// ===================== MAIN =====================
int main() {
    cout << "\n╔════════════════════════════════╗\n║   СИМУЛЯТОР СЕТИ КОФЕЕН       ║\n╚════════════════════════════════╝\n\n";
    string netname; cout << "название сети: "; getline(cin, netname);
    net network(netname, 500000);
    bool running = true; int choice;
    while (running) {
        cout << "\n╔════════════════════════════════╗\n║ 1. купить кофейню (300к)     ║\n║ 2. управление кофейней       ║\n║ 3. управление бариста        ║\n║ 4. показать все              ║\n║ 5. анализ сети               ║\n║ 6. РЕАЛЬНЫЙ ДЕНЬ (30 сек)    ║\n║ 7. БЫСТРАЯ НЕДЕЛЯ (7 дней)   ║\n║ 0. выход                     ║\n╚════════════════════════════════╝\nвыбор: ";
        cin >> choice;
        try {
            switch (choice) {
            case 1: { if (!network.can_afford_shop()) { cout << "❌ недостаточно средств! Нужно 300 000 руб.\n"; break; }
                string n; int flow, h; cout << "название: "; cin.ignore(); getline(cin, n); cout << "поток гостей: "; cin >> flow; cout << "часы работы: "; cin >> h;
                shop ns(n, flow, h); ns.addmenu(bludo("латте", 250, 4)); ns.addmenu(bludo("эспрессо", 150, 2)); ns.addmenu(bludo("круассан", 200, 1));
                network.buy_shop(ns); break; }
            case 2: { network.showall(); if (network.getshop(1)) { cout << "номер: "; int idx; cin >> idx; shop* s = network.getshop(idx); if (s) { bool m = true;
                while (m) { cout << "\n   1. инфо\n   2. детально\n   3. ++поток\n   4. нанять бариста\n   5. уволить\n   0. назад\nвыбор: "; int a; cin >> a;
                    if (a == 0) m = false; else if (a == 1) s->info(); else if (a == 2) s->info(true); else if (a == 3) ++(*s);
                    else if (a == 4) { string nm; double sal; int perf; cout << "имя: "; cin.ignore(); getline(cin, nm); cout << "зп: "; cin >> sal; cout << "производительность: "; cin >> perf; s->addb(barista(nm, sal, perf)); }
                    else if (a == 5) { if (s->getstaff().empty()) cout << "нет\n"; else { for (size_t i=0; i<s->getstaff().size(); i++) cout << i+1 << ". " << s->getstaff()[i].getname() << endl; int bi; cin >> bi;
                        if (bi>=1 && bi<=(int)s->getstaff().size()) s->delb(s->getstaff()[bi-1].getname()); } } } } } break; }
            case 3: { network.showall(); if (network.getshop(1)) { int idx; cin >> idx; shop* s = network.getshop(idx); if (s && !s->getstaff().empty()) {
                for (size_t i=0; i<s->getstaff().size(); i++) cout << i+1 << ". " << s->getstaff()[i].getname() << endl; int bi; cin >> bi;
                if (bi>=1 && bi<=(int)s->getstaff().size()) { barista& b = s->getstaff()[bi-1];
                cout << "1. ++производительность\n2. работать\nвыбор: "; int op; cin >> op; if (op == 1) ++b; else if (op == 2) b.work(); } } } break; }
            case 4: network.showall(); break;
            case 5: network.cheking(); break;
            case 6: network.liverealtime(1, 30); break;
            case 7: network.livefast(7); break;
            case 0: running = false; cout << "до свидания\n"; break;
            default: cout << "неверно\n";
            }
        } catch (const exception& e) { cerr << "ошибка: " << e.what() << endl; cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n'); }
    }
    return 0;
}