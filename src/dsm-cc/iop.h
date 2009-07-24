#ifndef _iop_ior
#define _iop_ior

struct biop_profile_body;

/* IOP::TaggedProfile() */
struct iop_tagged_profile {
	struct biop_profile_body *profile_body;
	struct lite_options_profile_body {
		int not_implemented;
	} *lite_body;
};

/* IOP::IOR() */
struct iop_ior {
	uint32_t type_id_length;
	char *type_id;
	char alignment_gap[4];
	uint32_t tagged_profiles_count;
	struct iop_tagged_profile *tagged_profiles;
};

void iop_free_ior(struct iop_ior *ior);
int iop_create_ior_dentries(struct dentry *parent, struct iop_ior *ior);
int iop_parse_ior(struct iop_ior *ior, const char *payload, uint32_t len);

int iop_parse_tagged_profiles(struct iop_tagged_profile *profile, 
		uint32_t count, const char *buf, uint32_t len);
int iop_create_tagged_profiles_dentries(struct dentry *parent,
		struct iop_tagged_profile *profile);


#endif /* _iop_ior */
