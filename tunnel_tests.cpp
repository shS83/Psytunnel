#include "tunnel_math.hpp"
#include <iostream>

using namespace tunnel;

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        // Tuhansia kameran asentoja, myös huojunnan ääripäissä: seinän
        // läpäiseminen tai vino kantavektori rikkoisi koko säteenleikkauksen.
        double minRadius=1, maxRadius=0;
        for (double sway: {0.0,0.5,1.0}) {
            for (int i=0; i<20000; ++i) {
                const double t=i*0.031;
                const auto c=cameraAt(t,t*1.5,sway);
                const double radius=std::hypot(c.position.x,c.position.y);
                expect(radius<=0.80000001,"Kamera meni tunnelin seinän läpi");
                expect(std::abs(std::hypot(c.target.x,c.target.y)-0.67*sway)<1e-9,
                       "Katseen kohde poistui ympyrän kehältä");
                for (Vec3 v: {c.forward,c.right,c.up})
                    expect(std::abs(dot(v,v)-1)<1e-9,"Kameran suuntavektori ei ole yksikkövektori");
                expect(std::abs(dot(c.forward,c.right))<1e-9 &&
                       std::abs(dot(c.forward,c.up))<1e-9 &&
                       std::abs(dot(c.right,c.up))<1e-9,"Kameran akselit eivät ole kohtisuorassa");
                expect(c.forward.z>0.8,"Kamera kääntyi pois tunnelin kulkusuunnasta");
                const auto next=cameraAt(t+0.0001,t*1.5+0.00015,sway);
                const Vec3 delta=next.position-c.position;
                expect(dot(delta,delta)<1e-6,"Kameran liikkeessä on hyppäys");
                if (sway==1) { minRadius=std::min(minRadius,radius); maxRadius=std::max(maxRadius,radius); }
                if (sway==0) expect(radius==0 && std::abs(c.forward.z-1)<1e-12,
                                    "Huojunnan nollaus ei keskitä kameraa");
            }
        }
        expect(minRadius<0.07 && maxRadius>0.79,"Kamera ei liiku keskustan ja reunan välillä");
        for (int cycle=0; cycle<18; ++cycle) {
            const double start=cycle*14.0;
            const auto first=paletteAt(start), middle=paletteAt(start+12), last=paletteAt(start+14-1e-6);
            const auto next=paletteAt(start+14);
            expect(first.from==cycle%6 && first.mix==0,"Värikierto alkaa väärin");
            expect(std::abs(middle.mix-0.5)<1e-9,"Värihäivytyksen puoliväli on väärä");
            expect(last.to==next.from && last.mix>0.99999 && next.mix==0,"Väri napsahtaa jakson rajalla");
        }
        const auto r=parseResolution("1920x1080");
        expect(r.width==1920 && r.height==1080,"Resoluution jäsennys epäonnistui");
        for (const auto* bad: {"", "0x0", "1920x", "x1080", "1920x1080oops", "-1x720", "99999999999x480"}) {
            bool rejected=false;
            try { (void)parseResolution(bad); } catch (const std::runtime_error&) { rejected=true; }
            expect(rejected,"Virheellinen resoluutio hyväksyttiin");
        }
        std::cout << "Kamera, liikkeen jatkuvuus, paletit ja resoluutiot OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
