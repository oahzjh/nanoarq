#include "nanoarq_in_test_project.h"
#include "nanoarq_hook_plugin.h"

#include <CppUTest/TestHarness.h>
#include <CppUTestExt/MockSupport.h>
#include <cstring>

TEST_GROUP(frame) {};

namespace
{

TEST(frame, size_is_header_size_plus_segment_length_plus_cobs_overhead)
{
    int seg_len = 123;
    CHECK_EQUAL(NANOARQ_FRAME_COBS_OVERHEAD + NANOARQ_FRAME_HEADER_SIZE + seg_len,
                arq__frame_size(seg_len));
}

struct Fixture
{
    Fixture()
    {
        h.version = 12;
        h.seg_len = 0;
        h.win_size = 9;
        h.seq_num = 123;
        h.msg_len = 5;
        h.seg_id = 3;
        h.ack_num = 23;
        h.ack_seg_mask = 0x0AC3;
        h.rst = 1;
        h.fin = 1;
        CHECK((int)sizeof(frame) >= arq__frame_size(h.seg_len));
    }
    arq__frame_hdr_t h;
    char const seg[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    char frame[64];
};

void MockArqFrameHdrWrite(arq__frame_hdr_t *frame_hdr, void *out_buf)
{
    mock().actualCall("arq__frame_hdr_write").withParameter("frame_hdr", frame_hdr).withParameter("out_buf", out_buf);
}

TEST(frame, write_writes_frame_header_at_offset_1)
{
    Fixture f;
    NANOARQ_MOCK_HOOK(arq__frame_hdr_write, MockArqFrameHdrWrite);
    mock().expectOneCall("arq__frame_hdr_write")
          .withParameter("frame_hdr", &f.h)
          .withParameter("out_buf", (void *)&f.frame[1]);
    arq__frame_write(&f.h, nullptr, f.frame, sizeof(f.frame));
}

TEST(frame, write_copies_segment_into_frame_after_header)
{
    Fixture f;
    f.h.seg_len = sizeof(f.seg);
    arq__frame_write(&f.h, f.seg, f.frame, sizeof(f.frame));
    MEMCMP_EQUAL(f.seg, &f.frame[1 + NANOARQ_FRAME_HEADER_SIZE], sizeof(f.seg));
}

void MockArqFrameHdrRead(void const *buf, arq__frame_hdr_t *out_frame_hdr)
{
    mock().actualCall("arq__frame_hdr_read").withParameter("buf", buf).withParameter("out_frame_hdr", out_frame_hdr);
}

TEST(frame, read_reads_frame_header_at_offset_1)
{
    Fixture f;
    NANOARQ_MOCK_HOOK(arq__frame_hdr_read, MockArqFrameHdrRead);
    mock().expectOneCall("arq__frame_hdr_read")
          .withParameter("buf", (void const *)&f.frame[1])
          .ignoreOtherParameters();
    void const *seg;
    arq__frame_read(f.frame, &f.h, &seg);
}

TEST(frame, read_points_out_seg_to_segment_in_frame)
{
    Fixture f;
    f.h.seg_len = sizeof(f.seg);
    arq__frame_hdr_write(&f.h, &f.frame[1]);
    std::memcpy(&f.frame[1 + NANOARQ_FRAME_HEADER_SIZE], f.seg, sizeof(f.seg));
    void const *rseg;
    arq__frame_hdr_t rh;
    arq__frame_read(f.frame, &rh, &rseg);
    CHECK_EQUAL(rseg, (void const *)&f.frame[1 + NANOARQ_FRAME_HEADER_SIZE]);
}

}

