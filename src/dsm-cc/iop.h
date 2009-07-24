#ifndef _iop_ior
#define _iop_ior

/* IOP::IOR() */
struct iop_ior {
	uint32_t type_id_length;
	char *type_id;
	char alignment_gap[4];
	uint32_t tagged_profiles_count;
	struct biop_tagged_profile *tagged_profiles;
};

void iop_free_ior(struct iop_ior *ior);
int iop_create_ior_dentries(struct dentry *parent, struct iop_ior *ior);
int iop_parse_ior(struct iop_ior *ior, const char *payload, uint32_t len);

#endif /* _iop_ior */
