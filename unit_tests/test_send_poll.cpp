#include "nanoarq_in_test_project.h"
#include "nanoarq_hook_plugin.h"
#include <CppUTest/TestHarness.h>
#include <CppUTestExt/MockSupport.h>
#include <array>

TEST_GROUP(send_poll) {};

namespace {

arq_uint32_t StubChecksum(void const *, int)
{
    return 0;
}

void MockSendWndStep(arq__send_wnd_t *w, arq_time_t dt)
{
    mock().actualCall("arq__send_wnd_step").withParameter("w", w).withParameter("dt", dt);
}

int MockSendWndPtrNext(arq__send_wnd_ptr_t *p, arq__send_wnd_t *w)
{
    return mock().actualCall("arq__send_wnd_ptr_next")
                 .withParameter("p", p)
                 .withParameter("w", w)
                 .returnIntValue();
}

void MockSendWndSeg(arq__send_wnd_t *w, int msg, int seg, void const **out_seg, int *out_seg_len)
{
    mock().actualCall("arq__send_wnd_seg")
          .withParameter("w", w)
          .withParameter("msg", msg)
          .withParameter("seg", seg)
          .withOutputParameter("out_seg", (void *)out_seg)
          .withOutputParameter("out_seg_len", out_seg_len);
}

int MockFrameWrite(arq__frame_hdr_t const *h,
                   void const *seg,
                   arq_checksum_cb_t checksum,
                   void *out_frame,
                   int frame_max)
{
    return mock().actualCall("arq__frame_write")
                 .withParameter("h", (void const *)h)
                 .withParameter("seg", seg)
                 .withParameter("checksum", (void *)checksum)
                 .withParameter("out_frame", out_frame)
                 .withParameter("frame_max", frame_max)
                 .returnIntValue();
}

struct Fixture
{
    Fixture()
    {
        w.msg = m.data();
        arq__send_wnd_init(&w, m.size(), 128, 16, 37);
        arq__send_frame_init(&f, 128);
        arq__send_wnd_ptr_init(&p);
        ARQ_MOCK_HOOK(arq__send_wnd_step, MockSendWndStep);
        ARQ_MOCK_HOOK(arq__send_wnd_ptr_next, MockSendWndPtrNext);
        ARQ_MOCK_HOOK(arq__send_wnd_seg, MockSendWndSeg);
        ARQ_MOCK_HOOK(arq__frame_write, MockFrameWrite);
    }

    arq__send_wnd_t w;
    arq__send_frame_t f;
    arq__send_wnd_ptr_t p;
    arq__frame_hdr_t h;
    std::array< arq__msg_t, 16 > m;
};

TEST(send_poll, calls_step_with_dt)
{
    Fixture f;
    arq_time_t const dt = 1234;
    mock().expectOneCall("arq__send_wnd_step").withParameter("w", &f.w).withParameter("dt", dt);
    mock().ignoreOtherCalls();
    arq__send_poll(&f.w, &f.p, &f.f, &f.h, &StubChecksum, dt);
}

TEST(send_poll, returns_zero_after_stepping_if_send_ptr_held_by_user_and_ptr_valid)
{
    Fixture f;
    f.p.valid = 1;
    f.f.state = ARQ__SEND_FRAME_STATE_HELD;
    mock().expectOneCall("arq__send_wnd_step").ignoreOtherParameters();
    int const written = arq__send_poll(&f.w, &f.p, &f.f, &f.h, &StubChecksum, 10);
    CHECK_EQUAL(0, written);
}

TEST(send_poll, returns_zero_after_stepping_if_send_ptr_not_acquired_and_ptr_valid)
{
    Fixture f;
    f.p.valid = 1;
    f.f.state = ARQ__SEND_FRAME_STATE_FREE;
    mock().expectOneCall("arq__send_wnd_step").ignoreOtherParameters();
    int const written = arq__send_poll(&f.w, &f.p, &f.f, &f.h, &StubChecksum, 10);
    CHECK_EQUAL(0, written);
}

struct ExpectSendWndStepFixture : Fixture
{
    ExpectSendWndStepFixture()
    {
        mock().expectOneCall("arq__send_wnd_step").ignoreOtherParameters();
    }
};

TEST(send_poll, advances_send_wnd_ptr_if_frame_is_ready)
{
    ExpectSendWndStepFixture f;
    mock().expectOneCall("arq__send_wnd_ptr_next").withParameter("p", &f.p).withParameter("w", &f.w);
    arq__send_poll(&f.w, &f.p, &f.f, &f.h, &StubChecksum, 1);
}

TEST(send_poll, doesnt_reset_message_retransmission_timer_if_havent_finished_message)
{
    ExpectSendWndStepFixture f;
    f.w.msg[0].rtx = 1234;
    mock().expectOneCall("arq__send_wnd_ptr_next").ignoreOtherParameters().andReturnValue(0);
    arq__send_poll(&f.w, &f.p, &f.f, &f.h, &StubChecksum, 1);
    CHECK_EQUAL(1234, f.w.msg[0].rtx);

}

TEST(send_poll, resets_message_retransmission_timer_if_finished_sending_message)
{
    ExpectSendWndStepFixture f;
    f.w.msg[0].rtx = 0;
    mock().expectOneCall("arq__send_wnd_ptr_next").ignoreOtherParameters().andReturnValue(1);
    arq__send_poll(&f.w, &f.p, &f.f, &f.h, &StubChecksum, 1);
    CHECK_EQUAL(f.w.rtx, f.w.msg[0].rtx);
}

struct ExpectSendWndPtrNextFixture : ExpectSendWndStepFixture
{
    ExpectSendWndPtrNextFixture()
    {
        mock().expectOneCall("arq__send_wnd_ptr_next").ignoreOtherParameters().andReturnValue(0);
    }
};

TEST(send_poll, resets_send_frame)
{
    ExpectSendWndPtrNextFixture f;
    f.f.len = 1;
    f.f.state = ARQ__SEND_FRAME_STATE_RELEASED;
    arq__send_poll(&f.w, &f.p, &f.f, &f.h, &StubChecksum, 1);
    CHECK_EQUAL(0, f.f.len);
    CHECK_EQUAL(ARQ__SEND_FRAME_STATE_FREE, f.f.state);
}

TEST(send_poll, returns_zero_if_no_new_data_to_send)
{
    ExpectSendWndPtrNextFixture f;
    int const written = arq__send_poll(&f.w, &f.p, &f.f, &f.h, &StubChecksum, 1);
    CHECK_EQUAL(0, written);
}

TEST(send_poll, gets_segment_pointer_from_send_wnd_seg_if_sending_next_seg)
{
    ExpectSendWndPtrNextFixture f;
    f.p.valid = 1;
    f.f.state = ARQ__SEND_FRAME_STATE_RELEASED;
    f.p.msg = 123;
    f.p.seg = 456;
    mock().expectOneCall("arq__send_wnd_seg")
          .withParameter("w", &f.w)
          .withParameter("msg", f.p.msg)
          .withParameter("seg", f.p.seg)
          .ignoreOtherParameters();
    mock().ignoreOtherCalls();
    arq__send_poll(&f.w, &f.p, &f.f, &f.h, &StubChecksum, 1);
}

TEST(send_poll, calls_frame_write_if_sending_next_seg)
{
    ExpectSendWndPtrNextFixture f;
    f.p.valid = 1;
    f.f.state = ARQ__SEND_FRAME_STATE_RELEASED;
    void *seg = (void *)0x12345678;
    int seg_len = 432;
    mock().expectOneCall("arq__send_wnd_seg")
          .withOutputParameterReturning("out_seg", &seg, sizeof(void *))
          .withOutputParameterReturning("out_seg_len", &seg_len, sizeof(int))
          .ignoreOtherParameters();
    mock().expectOneCall("arq__frame_write")
          .withParameter("h", (void const *)&f.h)
          .withParameter("seg", (void const *)seg)
          .withParameter("checksum", (void *)&StubChecksum)
          .withParameter("out_frame", f.f.buf)
          .withParameter("frame_max", f.f.cap);
    mock().ignoreOtherCalls();
    arq__send_poll(&f.w, &f.p, &f.f, &f.h, &StubChecksum, 1);
    CHECK_EQUAL(seg_len, f.h.seg_len);
}

TEST(send_poll, sets_frame_length_if_sending_next_seg)
{
    ExpectSendWndPtrNextFixture f;
    f.p.valid = 1;
    f.f.state = ARQ__SEND_FRAME_STATE_RELEASED;
    mock().expectOneCall("arq__frame_write").ignoreOtherParameters().andReturnValue(123);
    mock().ignoreOtherCalls();
    arq__send_poll(&f.w, &f.p, &f.f, &f.h, &StubChecksum, 1);
    CHECK_EQUAL(123, f.f.len);
}

}
