#ifndef	_MBINDEX_H
#define	_MBINDEX_H

/* $Id$ */

typedef	struct	_fd_list {
    struct _fd_list	*next;
    char		fname[65];
    time_t		fdate;
} fd_list;


int	lockindex(void);
void	ulockindex(void);
void	Help(void);
void	ProgName(void);
void	die(int);
char	*fullpath(char *);
int	compile(char *, unsigned short, unsigned short, unsigned short);
void	tidy_fdlist(fd_list **);
void	fill_fdlist(fd_list **, char *, time_t);
void	sort_fdlist(fd_list **);
char	*pull_fdlist(fd_list **);
int	makelist(char *, unsigned short, unsigned short, unsigned short);
void	tidy_nllist(nl_list **);
int	in_nllist(struct _nlidx, nl_list **, int);
void	fill_nllist(struct _nlidx, nl_list **);
int	comp_node(nl_list **, nl_list **);
int	nodebld(void);

#endif

