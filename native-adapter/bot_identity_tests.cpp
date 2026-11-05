#include "bot_identity.hpp"
#include <cstring>
#include <iostream>
int main() {
    using namespace bf4_takeover;
    BotIdentitySnapshot saved;
    saved.player=10;saved.context=20;saved.input_cookie=30;saved.id=21;saved.observed_at=100;
    std::strcpy(saved.name.data(),"Recorded Bot");
    BotIdentityLive live;
    live.player=10;live.context_owner=10;live.roster_owner=10;live.input_cookie=30;live.id=21;live.now=200;
    live.name=saved.name;
    if(!matches_live_bot(saved,live))return 1;
    // Real failure pattern: control switches away from the generic callback,
    // but the same live bot context continues to own the same player.
    live.now=178819;
    if(!matches_live_bot(saved,live)){
        std::cerr<<"RED: unchanged bot ownership expires after switching control mode\n";return 2;
    }
    for(int change=0;change<6;++change){
        auto wrong=live;
        switch(change){case 0:wrong.context_owner=99;break;case 1:wrong.id++;break;
        case 2:wrong.name[0]='X';break;case 3:wrong.input_cookie++;break;case 4:wrong.player=99;break;case 5:wrong.roster_owner=99;break;}
        if(matches_live_bot(saved,wrong))return 3;
    }
    std::cout<<"GREEN: ownership survives mode changes; reused/retired identities rejected\n";
}
