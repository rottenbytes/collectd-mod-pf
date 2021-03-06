void	 print_rule(struct pf_rule *, const char *, int, char *);
void	 print_pool(struct pf_pool *, u_int16_t, u_int16_t, sa_family_t, int, int, char *);
int	 pfctl_get_pool(int, struct pf_pool *, u_int32_t, u_int32_t, int,
	    char *);
void	 pfctl_clear_pool(struct pf_pool *);


#define PF_NAT_PROXY_PORT_LOW	50001
#define PF_NAT_PROXY_PORT_HIGH	65535

#define MAXRULESTRING	256
#define CURRENTPOS	rulestring + strlen(rulestring)
#define CURRENTSIZE	MAXRULESTRING - strlen(rulestring)
#define RULEPRINT(...)	snprintf(CURRENTPOS, CURRENTSIZE, __VA_ARGS__)
