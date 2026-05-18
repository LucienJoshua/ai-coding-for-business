#include <sys/time.h>
#include <errno.h>
#include <semaphore.h>
#include "tls_afx.h"
#include "tls_event.h"

void tls_event_initialize(tls_event_t* evt)
{
	if (!evt)
		return;

	pthread_mutex_init(&evt->mutex_, NULL);
	pipe(evt->fds_);
	if (evt->state_)
	{
		evt->state_ = TLS_EVENT_STATE_PASSIVE;
		tls_event_set(evt);
	}
}
void tls_event_uninitialize(tls_event_t* evt)
{
	if (!evt)
		return;

	pthread_mutex_destroy(&evt->mutex_);
	close(evt->fds_[0]);
	close(evt->fds_[1]);
}

void tls_event_wait(tls_event_t* evt)
{
	int		ret;
	fd_set	fd;

	if (!evt)
		return;
	FD_ZERO(&fd);
	FD_SET(evt->fds_[0], &fd);
	ret = select(evt->fds_[0]+1, &fd, NULL, NULL, NULL);
	if (ret == 1)
	{
		if (evt->auto_reset_)
		{
			tls_event_reset(evt);
		}
	}
}
int	tls_event_timewait(tls_event_t* evt, int timeout)
{
	int					ret;
	struct timeval		tv;
	fd_set				fd;

	if (!evt)
		return 0;

	if (timeout > 0)
	{
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;

		FD_ZERO(&fd);
		FD_SET(evt->fds_[0], &fd);
		ret = select(evt->fds_[0]+1, &fd, NULL, NULL, &tv);
		if (ret == 1)
		{
			if (evt->auto_reset_)
				tls_event_reset(evt);
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		tls_event_wait(evt);
	}

	return 1;
}
int	tls_event_trywait(tls_event_t* evt)
{
	return tls_event_timewait(evt, 0);
}

void tls_event_set(tls_event_t* evt)
{
	char buf[] = "event: set";
	if (NULL == evt)
		return;
	pthread_mutex_lock(&evt->mutex_);
	if (!evt->state_)
	{
		evt->state_ = 1;
		write(evt->fds_[1], buf, sizeof(buf));
	}
	pthread_mutex_unlock(&evt->mutex_);
}
void tls_event_reset(tls_event_t* evt)
{
	char buf[] = "event: reset";
	if (NULL == evt)
		return;
	pthread_mutex_lock(&evt->mutex_);
	if (evt->state_)
	{
		read(evt->fds_[0], buf, sizeof(buf));
	}
	evt->state_ = 0;
	pthread_mutex_unlock(&evt->mutex_);
}
