void PrintUsage(int argc, char * argv[]) {
	if (argc >=1) {
		printf("Usage: %s -h -n\n", argv[0]);
		printf("  Options:\n");
		printf("      -n Don't fort off as a daemon.\n");
		printf("      -t Show this help screen.\n");
		printf("\n");
	}
}
