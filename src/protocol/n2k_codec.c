#include "n2k_codec.h"

#include <string.h>

bool n2k_is_fast_packet(uint32_t pgn)
{
    switch (pgn) {
    case 127237: /* Heading/Track Control */
    case 130845: /* Simnet: Key Value */
    case 130850: /* Simnet: AP Command */
    case 130851: /* Simnet: AP Command Reply */
        return true;
    default:
        return false;
    }
}

void n2k_fp_pool_init(n2k_fp_pool_t *pool)
{
    if (pool != NULL) {
        memset(pool, 0, sizeof(*pool));
    }
}

static n2k_fp_slot_t *find_slot(n2k_fp_pool_t *pool, uint32_t pgn, uint8_t src)
{
    for (size_t i = 0; i < N2K_FASTPACKET_SLOTS; i++) {
        if (pool->slots[i].active && pool->slots[i].pgn == pgn &&
            pool->slots[i].src == src) {
            return &pool->slots[i];
        }
    }
    return NULL;
}

static n2k_fp_slot_t *alloc_slot(n2k_fp_pool_t *pool, uint32_t now_ms)
{
    n2k_fp_slot_t *oldest = &pool->slots[0];

    for (size_t i = 0; i < N2K_FASTPACKET_SLOTS; i++) {
        if (!pool->slots[i].active) {
            return &pool->slots[i];
        }
        if ((int32_t)(now_ms - pool->slots[i].last_ms) >
            (int32_t)(now_ms - oldest->last_ms)) {
            oldest = &pool->slots[i];
        }
    }
    return oldest;
}

n2k_fp_result_t n2k_fp_feed(n2k_fp_pool_t *pool,
                            uint32_t id,
                            const uint8_t *data,
                            uint8_t dlc,
                            uint32_t now_ms,
                            uint8_t *out,
                            size_t out_cap,
                            size_t *out_len)
{
    if (pool == NULL || data == NULL || out == NULL || out_len == NULL) {
        return N2K_FP_ERROR;
    }
    *out_len = 0;

    const uint32_t pgn = n2k_id_pgn(id);
    if (!n2k_is_fast_packet(pgn)) {
        return N2K_FP_NOT_FAST;
    }
    if (dlc < 2) {
        return N2K_FP_ERROR;
    }

    const uint8_t src = n2k_id_src(id);
    const uint8_t seq = (uint8_t)(data[0] >> 5);
    const uint8_t frame = (uint8_t)(data[0] & 0x1Fu);

    if (frame == 0) {
        const uint16_t total = data[1];
        if (total == 0 || total > N2K_MAX_PAYLOAD) {
            return N2K_FP_ERROR;
        }

        n2k_fp_slot_t *slot = find_slot(pool, pgn, src);
        if (slot == NULL) {
            slot = alloc_slot(pool, now_ms);
        }

        memset(slot, 0, sizeof(*slot));
        slot->active = true;
        slot->pgn = pgn;
        slot->src = src;
        slot->seq = seq;
        slot->next_frame = 1;
        slot->total_len = total;
        slot->last_ms = now_ms;

        const size_t copy = (size_t)(dlc - 2);
        slot->len = (uint16_t)(copy > total ? total : copy);
        memcpy(slot->data, data + 2, slot->len);

        if (slot->len >= total) {
            if (out_cap < total) {
                slot->active = false;
                return N2K_FP_ERROR;
            }
            memcpy(out, slot->data, total);
            *out_len = total;
            slot->active = false;
            return N2K_FP_COMPLETE;
        }
        return N2K_FP_INCOMPLETE;
    }

    n2k_fp_slot_t *slot = find_slot(pool, pgn, src);
    if (slot == NULL) {
        /* First frame missed: drop the rest instead of mixing messages. */
        return N2K_FP_INCOMPLETE;
    }
    if (slot->seq != seq || slot->next_frame != frame) {
        slot->active = false;
        return N2K_FP_ERROR;
    }

    const size_t copy = (size_t)(dlc - 1);
    size_t room = slot->total_len - slot->len;
    if (copy < room) {
        room = copy;
    }
    memcpy(slot->data + slot->len, data + 1, room);
    slot->len = (uint16_t)(slot->len + room);
    slot->next_frame = (uint8_t)(slot->next_frame + 1u);
    slot->last_ms = now_ms;

    if (slot->len >= slot->total_len) {
        if (out_cap < slot->total_len) {
            slot->active = false;
            return N2K_FP_ERROR;
        }
        memcpy(out, slot->data, slot->total_len);
        *out_len = slot->total_len;
        slot->active = false;
        return N2K_FP_COMPLETE;
    }

    return N2K_FP_INCOMPLETE;
}

void n2k_fp_expire(n2k_fp_pool_t *pool, uint32_t now_ms, uint32_t max_age_ms)
{
    if (pool == NULL) {
        return;
    }
    for (size_t i = 0; i < N2K_FASTPACKET_SLOTS; i++) {
        if (pool->slots[i].active &&
            (uint32_t)(now_ms - pool->slots[i].last_ms) > max_age_ms) {
            pool->slots[i].active = false;
        }
    }
}
