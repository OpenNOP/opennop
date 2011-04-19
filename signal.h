void signal_handler(int sig) 
{

	switch(sig){
		case SIGHUP:
			syslog(LOG_WARNING, "Received SIGHUP signal.");
			break;
		case SIGTERM:
			syslog(LOG_WARNING, "Received SIGTERM signal.");
			
			switch(servicestate){
				case RUNNING:
					servicestate = STOPPING;
					break;
				case STOPPING:
					servicestate = STOPPED;
					break;
				default:
					servicestate = STOPPED;
					break;
			}
			
			break;
		case SIGQUIT:
			syslog(LOG_WARNING, "Received SIGQUIT signal.");
			break;
		default:
			syslog(LOG_WARNING, "Unhandled signal (%s)", strsignal(sig));
			break;
	}
}

