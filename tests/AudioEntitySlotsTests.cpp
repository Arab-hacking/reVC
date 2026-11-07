#include <cassert>
#include <cstdint>
#include <vector>
#include "../src/audio/AudioEntitySlots.h"

struct AudioEntity { bool used; int marker; };

int main()
{
    std::vector<AudioEntity> entities(250);
    std::vector<uint32_t> order(250);
    for(std::size_t i = 0; i < 250; ++i){
        entities[i].used = true;
        entities[i].marker = static_cast<int>(i);
        order[i] = static_cast<uint32_t>(249 - i);
    }
    bool grew = GrowAudioEntitySlots(entities, order, 128, 1000);
    assert(grew);
    if(!grew) return 1;
    assert(entities.size() == 378 && order.size() == 378);
    for(std::size_t i = 0; i < 250; ++i){
        assert(entities[i].used && entities[i].marker == static_cast<int>(i));
        assert(order[i] == 249 - i);
    }
    assert(!entities[250].used && order[250] == 378);
    entities[250].used = true;
    order[250] = 250;
    grew = GrowAudioEntitySlots(entities, order, 128, 1000);
    assert(grew);
    if(!grew) return 1;
    assert(entities[250].used && order[250] == 250);
    grew = GrowAudioEntitySlots(entities, order, 128, 505);
    assert(!grew);
    if(grew) return 1;
    assert(entities.size() == 506 && order.size() == 506);
    order.pop_back();
    grew = GrowAudioEntitySlots(entities, order, 128, 1000);
    assert(!grew);
    if(grew) return 1;
    assert(entities.size() == 506 && order.size() == 505);
}
