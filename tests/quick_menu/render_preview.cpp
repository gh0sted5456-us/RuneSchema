// Export the actual portable Draw commands. Icons are explicitly placeholders;
// no game textures or on-screen Unreal output are available to this renderer.
#include "Generator/QuickMenuUI.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace PS::QuickUI;
std::string Esc(const std::string& s){std::string r;for(char c:s){switch(c){case '&':r+="&amp;";break;case '<':r+="&lt;";break;case '>':r+="&gt;";break;case '"':r+="&quot;";break;default:r+=c;}}return r;}
std::string Colour(const std::array<float,4>& c){std::ostringstream s;s<<"rgb("<<int(c[0]*255)<<","<<int(c[1]*255)<<","<<int(c[2]*255)<<")";return s.str();}
Catalog Example(){Catalog c;c.authority=true;c.players={{"/Example.PC","Local player",true}};
    const std::vector<std::string> names={"Copper ore","Air rune","Clean water","Coarse animal fur","Ash logs","Iron ore","Law rune","Stone","Nature rune","Gold ore"};
    for(size_t i=0;i<names.size();++i){auto p="/Example/Item"+std::to_string(i)+".Item"+std::to_string(i);c.entries[0].push_back({p,names[i],p,"/Placeholder/Icon.Icon",names[i]});}
    for(const auto* n:{"Copper node - medium","Copper node - large","Granite","Iron node - medium","Rune essence","Stone node","Tin node - medium","Tin node - large"}){auto p="/Example/Node"+std::to_string(c.entries[2].size())+".Node_C";c.entries[2].push_back({"@loaded-resource:"+p,n,p,{},n});}
    for(const auto* n:{"Goblin - melee","Goblin - ranged","Garou - warrior","Skeleton - ranger","Skeleton - necromancer","Rat","Goat","Dragon"}){auto p="/Example/AI"+std::to_string(c.entries[1].size())+".AI_C";c.entries[1].push_back({"@loaded-ai:"+p,n,p,{},n});}return c;}
void Output(Model& m,const std::string& path){std::ofstream o(path);o<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"960\" height=\"700\" viewBox=\"0 0 960 700\"><rect width=\"960\" height=\"700\" fill=\"#202021\"/><g transform=\"translate(20 12)\">\n";
    for(const auto& d:m.Render().draws){const auto r=d.box;const auto col=Colour(d.color);
        if(d.kind==Draw::Kind::Rectangle)o<<"<rect x=\""<<r.x<<"\" y=\""<<r.y<<"\" width=\""<<r.w<<"\" height=\""<<r.h<<"\" fill=\""<<col<<"\" fill-opacity=\""<<d.color[3]<<"\"/>\n";
        else if(d.kind==Draw::Kind::Text)o<<"<text x=\""<<r.x<<"\" y=\""<<r.y+d.font*.80f<<"\" font-family=\"DejaVu Sans\" font-size=\""<<d.font<<"\" fill=\""<<col<<"\" fill-opacity=\""<<d.color[3]<<"\">"<<Esc(d.text)<<"</text>\n";
        else o<<"<rect x=\""<<r.x<<"\" y=\""<<r.y<<"\" width=\""<<r.w<<"\" height=\""<<r.h<<"\" rx=\"4\" fill=\"#64625e\"/><text x=\""<<r.x+r.w*.5<<"\" y=\""<<r.y+r.h*.64<<"\" font-family=\"DejaVu Sans\" text-anchor=\"middle\" font-size=\""<<r.h*.45<<"\" fill=\"#eee5cd\">I</text>\n";
    }
    o<<"</g><text x=\"20\" y=\"682\" fill=\"#d4c9b5\" font-family=\"DejaVu Sans\" font-size=\"13\">Headless layout preview - example rows and placeholder icons; not an in-game screenshot.</text></svg>\n";
}
int main(int argc,char**argv){if(argc!=2){std::cerr<<"Usage: render_preview output-folder/\n";return 2;}std::string base=argv[1];
    Model m;m.Update(Example());m.status="Ready. Select a tab or narrow the catalog with the filter.";m.catalogStatus="Game catalog: example entries shown for layout inspection.";Output(m,base+"items.svg");
    m.tab=Tab::Enemies;Output(m,base+"ai.svg");m.tab=Tab::Resources;Output(m,base+"resources.svg");m.Activate("node",m.catalog.entries[2][5].id);m.name="Custom stone node";m.scale="1.5";m.count="3";m.effect=Effect::None;
    for(int i=0;i<3;++i){m.Activate("add-loot",m.catalog.entries[0][i].path);}Output(m,base+"resource-form.svg");
    m.Activate("loot-picker");Output(m,base+"loot-picker.svg");
}
