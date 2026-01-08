#ifndef TASKDB_H
#define TASKDB_H

#include <string>
#include <vector>
#include <map>
#include <ctime>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

// Структуры для Task Flow
struct Task {
    string id;
    string title;
    string description;
    string status;        // "todo", "in_progress", "done", "review"
    string project_id;
    string assignee_id;   // user_id кто выполняет
    string creator_id;    // user_id кто создал
    vector<string> tags;
    time_t created_at;
    time_t due_date;
    time_t completed_at;
};

struct Project {
    string id;
    string name;
    string description;
    string owner_id;
    vector<string> member_ids;
    time_t created_at;
    time_t updated_at;
};

struct Comment {
    string id;
    string task_id;
    string user_id;
    string content;
    time_t created_at;
};

struct Notification {
    string id;
    string user_id;
    string type;          // "task_assigned", "deadline", "comment", "status_change"
    string message;
    string entity_id;     // task_id или project_id
    bool read;
    time_t created_at;
};

class TaskDB {
private:
    string db_file;
    json data;
    
    void loadDB();
    void saveDB();
    string generateId();
    time_t getCurrentTime();
    
public:
    TaskDB(const string& db_file = "taskflow_db.json");
    void initializeDB();
    
    // Project operations
    Project createProject(const string& name, 
                         const string& description,
                         const string& owner_id);
    
    Project getProject(const string& project_id);
    vector<Project> getUserProjects(const string& user_id);
    vector<Project> getAllProjects();
    bool updateProject(const string& project_id, 
                      const map<string, string>& updates);
    bool deleteProject(const string& project_id);
    bool addProjectMember(const string& project_id, const string& user_id);
    bool removeProjectMember(const string& project_id, const string& user_id);
    bool isProjectMember(const string& project_id, const string& user_id);
    bool isProjectOwner(const string& project_id, const string& user_id);
    
    // Task operations
    Task createTask(const string& title,
                   const string& description,
                   const string& project_id,
                   const string& creator_id,
                   const string& assignee_id = "",
                   time_t due_date = 0);
    
    Task getTask(const string& task_id);
    vector<Task> getProjectTasks(const string& project_id);
    vector<Task> getUserTasks(const string& user_id);
    vector<Task> getUserAssignedTasks(const string& user_id);
    bool updateTask(const string& task_id, 
                   const map<string, string>& updates);
    bool updateTaskStatus(const string& task_id, const string& status);
    bool assignTask(const string& task_id, const string& assignee_id);
    bool deleteTask(const string& task_id);
    
    // Comment operations
    Comment addComment(const string& task_id, 
                      const string& user_id,
                      const string& content);
    vector<Comment> getTaskComments(const string& task_id);
    bool deleteComment(const string& comment_id);
    
    // Notification operations
    Notification createNotification(const string& user_id,
                                  const string& type,
                                  const string& message,
                                  const string& entity_id = "");
    vector<Notification> getUserNotifications(const string& user_id, bool unread_only = false);
    bool markNotificationRead(const string& notification_id);
    bool markAllNotificationsRead(const string& user_id);
    
    // Search operations
    vector<Task> searchTasks(const string& project_id, 
                            const string& query,
                            const string& status = "",
                            const string& assignee_id = "");
    
    // Stats operations
    map<string, int> getProjectStats(const string& project_id);
    map<string, int> getUserStats(const string& user_id);
    
    // Tag operations
    bool addTaskTag(const string& task_id, const string& tag);
    bool removeTaskTag(const string& task_id, const string& tag);
    vector<string> getTaskTags(const string& task_id);
    vector<Task> getTasksByTag(const string& project_id, const string& tag);
};

#endif