#ifndef XADMIN_TIME_H
#define XADMIN_TIME_H

#include <xsbase.h>
#include <stdint.h>

/* Persisted application timestamps remain Unix microseconds. xtime is now
 * Gregorian UTC milliseconds; never write it directly into existing rows. */
static inline int64 XAdmin_UnixNowUs(void)
{
    int64 milliseconds = 0;
    if (!xrtTimeToUnixMs(xrtNow(), &milliseconds) ||
        milliseconds > INT64_MAX / 1000 || milliseconds < INT64_MIN / 1000) return 0;
    return milliseconds * 1000;
}

static inline bool XAdmin_TimeFromUnixUs(int64 microseconds, xtime* time)
{
    int64 milliseconds = microseconds / 1000;
    if (microseconds % 1000 < 0) --milliseconds;
    return xrtTimeFromUnixMs(milliseconds, time);
}

/* A monotonic budget spans all phases of one operation. Public xrt waits
 * accept remaining milliseconds, with zero meaning a nonblocking check. */
typedef double XAdminDeadline;

static inline uint64 XAdmin_MonotonicUs(void)
{
    return (uint64)(xrtTimer() * 1000000.0);
}

static inline XAdminDeadline XAdmin_DeadlineAfterMs(int64 milliseconds)
{
    return xrtTimer() + (milliseconds > 0 ? (double)milliseconds / 1000.0 : 0.0);
}

static inline int64 XAdmin_DeadlineRemainingMs(XAdminDeadline deadline)
{
    double remaining = (deadline - xrtTimer()) * 1000.0;
    int64 whole;
    if (remaining <= 0.0) return 0;
    if (remaining >= (double)INT64_MAX) return INT64_MAX;
    whole = (int64)remaining;
    return whole + (remaining > (double)whole ? 1 : 0);
}

static inline bool XAdmin_DeadlineExpired(XAdminDeadline deadline)
{
    return XAdmin_DeadlineRemainingMs(deadline) == 0;
}

#endif
