/*
 * robin_manager.h
 *
 * Header file containing public interface for Robin Manager.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_MANAGER_H
#define ROBIN_MANAGER_H

/**
 * @brief Manage the connection with the client
 *
 * @param id Connection ID (Robin Thread ID)
 * @param fd Socket file descriptor
 */
void robin_manage_connection(int id, int fd);

#endif /* ROBIN_MANAGER_H */
