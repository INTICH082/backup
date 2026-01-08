#include "../include/precompiled.h"
#include "../include/TaskDB.h"
#include <fstream>
#include <iostream>
#include <random>
#include <algorithm>
#include <ctime>

TaskDB::TaskDB(const string& db_file) : db_file(db_file) {
    loadDB();
}

void TaskDB::loadDB() {
    ifstream file(db_file);
    if (file.is_open()) {
        try {
            file >> data;
        } catch (...) {
            data = json::object();
            data["projects"] = json::array();
            data["tasks"] = json::array();
            data["comments"] = json::array();
            data["notifications"] = json::array();
        }
    } else {
        data = json::object();
        data["projects"] = json::array();
        data["tasks"] = json::array();
        data["comments"] = json::array();
        data["notifications"] = json::array();
        saveDB();
    }
}

void TaskDB::saveDB() {
    ofstream file(db_file);
    if (file.is_open()) {
        file << data.dump(4);
    }
}

string TaskDB::generateId() {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    string id;
    
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    
    for (int i = 0; i < 16; ++i) {
        id += charset[dis(gen)];
    }
    
    return id;
}

time_t TaskDB::getCurrentTime() {
    return time(nullptr);
}

void TaskDB::initializeDB() {
    if (!data.contains("projects")) {
        data["projects"] = json::array();
    }
    if (!data.contains("tasks")) {
        data["tasks"] = json::array();
    }
    if (!data.contains("comments")) {
        data["comments"] = json::array();
    }
    if (!data.contains("notifications")) {
        data["notifications"] = json::array();
    }
    saveDB();
    cout << "[TaskDB] Database initialized: " << db_file << endl;
}

Project TaskDB::createProject(const string& name, 
                             const string& description,
                             const string& owner_id) {
    Project project;
    project.id = generateId();
    project.name = name;
    project.description = description;
    project.owner_id = owner_id;
    project.member_ids.push_back(owner_id);
    project.created_at = getCurrentTime();
    project.updated_at = getCurrentTime();
    
    json project_json;
    project_json["id"] = project.id;
    project_json["name"] = name;
    project_json["description"] = description;
    project_json["owner_id"] = owner_id;
    project_json["member_ids"] = json::array();
    project_json["member_ids"].push_back(owner_id);
    project_json["created_at"] = project.created_at;
    project_json["updated_at"] = project.updated_at;
    
    data["projects"].push_back(project_json);
    saveDB();
    
    // Создаем уведомление для владельца
    createNotification(owner_id, "project_created", 
                      "Вы создали проект '" + name + "'", project.id);
    
    cout << "[TaskDB] Created project: " << name << " (ID: " << project.id << ")" << endl;
    return project;
}

Project TaskDB::getProject(const string& project_id) {
    for (const auto& project : data["projects"]) {
        if (project["id"] == project_id) {
            Project p;
            p.id = project["id"].get<string>();
            p.name = project["name"].get<string>();
            p.description = project["description"].get<string>();
            p.owner_id = project["owner_id"].get<string>();
            p.created_at = project["created_at"].get<time_t>();
            p.updated_at = project["updated_at"].get<time_t>();
            
            for (const auto& member : project["member_ids"]) {
                p.member_ids.push_back(member.get<string>());
            }
            
            return p;
        }
    }
    return Project{};
}

vector<Project> TaskDB::getUserProjects(const string& user_id) {
    vector<Project> projects;
    
    for (const auto& project : data["projects"]) {
        bool is_member = false;
        
        // Проверяем владельца
        if (project["owner_id"] == user_id) {
            is_member = true;
        }
        
        // Проверяем членов
        for (const auto& member : project["member_ids"]) {
            if (member == user_id) {
                is_member = true;
                break;
            }
        }
        
        if (is_member) {
            Project p;
            p.id = project["id"].get<string>();
            p.name = project["name"].get<string>();
            p.description = project["description"].get<string>();
            p.owner_id = project["owner_id"].get<string>();
            p.created_at = project["created_at"].get<time_t>();
            p.updated_at = project["updated_at"].get<time_t>();
            
            for (const auto& member : project["member_ids"]) {
                p.member_ids.push_back(member.get<string>());
            }
            
            projects.push_back(p);
        }
    }
    
    return projects;
}

vector<Project> TaskDB::getAllProjects() {
    vector<Project> projects;
    
    for (const auto& project : data["projects"]) {
        Project p;
        p.id = project["id"].get<string>();
        p.name = project["name"].get<string>();
        p.description = project["description"].get<string>();
        p.owner_id = project["owner_id"].get<string>();
        p.created_at = project["created_at"].get<time_t>();
        p.updated_at = project["updated_at"].get<time_t>();
        
        for (const auto& member : project["member_ids"]) {
            p.member_ids.push_back(member.get<string>());
        }
        
        projects.push_back(p);
    }
    
    return projects;
}

bool TaskDB::updateProject(const string& project_id, 
                          const map<string, string>& updates) {
    for (auto& project : data["projects"]) {
        if (project["id"] == project_id) {
            for (const auto& update : updates) {
                if (update.first == "name" || update.first == "description") {
                    project[update.first] = update.second;
                }
            }
            project["updated_at"] = getCurrentTime();
            saveDB();
            
            cout << "[TaskDB] Updated project: " << project_id << endl;
            return true;
        }
    }
    return false;
}

bool TaskDB::deleteProject(const string& project_id) {
    // Находим проект
    size_t project_index = data["projects"].size();
    for (size_t i = 0; i < data["projects"].size(); ++i) {
        if (data["projects"][i]["id"] == project_id) {
            project_index = i;
            break;
        }
    }
    
    if (project_index == data["projects"].size()) {
        return false;
    }
    
    // Удаляем связанные задачи
    vector<size_t> tasks_to_remove;
    for (size_t i = 0; i < data["tasks"].size(); ++i) {
        if (data["tasks"][i]["project_id"] == project_id) {
            tasks_to_remove.push_back(i);
        }
    }
    
    // Удаляем в обратном порядке
    for (auto it = tasks_to_remove.rbegin(); it != tasks_to_remove.rend(); ++it) {
        data["tasks"].erase(data["tasks"].begin() + *it);
    }
    
    // Удаляем проект
    data["projects"].erase(data["projects"].begin() + project_index);
    saveDB();
    
    cout << "[TaskDB] Deleted project: " << project_id << endl;
    return true;
}

bool TaskDB::addProjectMember(const string& project_id, const string& user_id) {
    for (auto& project : data["projects"]) {
        if (project["id"] == project_id) {
            // Проверяем, не является ли пользователь уже участником
            for (const auto& member : project["member_ids"]) {
                if (member == user_id) {
                    return true; // Уже участник
                }
            }
            
            project["member_ids"].push_back(user_id);
            project["updated_at"] = getCurrentTime();
            saveDB();
            
            // Создаем уведомление
            createNotification(user_id, "project_invite", 
                              "Вас добавили в проект '" + project["name"].get<string>() + "'", 
                              project_id);
            
            cout << "[TaskDB] Added member " << user_id << " to project: " << project_id << endl;
            return true;
        }
    }
    return false;
}

bool TaskDB::removeProjectMember(const string& project_id, const string& user_id) {
    for (auto& project : data["projects"]) {
        if (project["id"] == project_id) {
            // Нельзя удалить владельца
            if (project["owner_id"] == user_id) {
                return false;
            }
            
            // Ищем и удаляем пользователя
            json new_members = json::array();
            bool found = false;
            
            for (const auto& member : project["member_ids"]) {
                if (member != user_id) {
                    new_members.push_back(member);
                } else {
                    found = true;
                }
            }
            
            if (found) {
                project["member_ids"] = new_members;
                project["updated_at"] = getCurrentTime();
                saveDB();
                
                cout << "[TaskDB] Removed member " << user_id << " from project: " << project_id << endl;
                return true;
            }
        }
    }
    return false;
}

bool TaskDB::isProjectMember(const string& project_id, const string& user_id) {
    for (const auto& project : data["projects"]) {
        if (project["id"] == project_id) {
            // Проверяем владельца
            if (project["owner_id"] == user_id) {
                return true;
            }
            
            // Проверяем участников
            for (const auto& member : project["member_ids"]) {
                if (member == user_id) {
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

bool TaskDB::isProjectOwner(const string& project_id, const string& user_id) {
    for (const auto& project : data["projects"]) {
        if (project["id"] == project_id) {
            return project["owner_id"] == user_id;
        }
    }
    return false;
}

Task TaskDB::createTask(const string& title,
                       const string& description,
                       const string& project_id,
                       const string& creator_id,
                       const string& assignee_id,
                       time_t due_date) {
    Task task;
    task.id = generateId();
    task.title = title;
    task.description = description;
    task.status = "todo";
    task.project_id = project_id;
    task.creator_id = creator_id;
    task.assignee_id = assignee_id;
    task.created_at = getCurrentTime();
    task.due_date = due_date;
    task.completed_at = 0;
    
    json task_json;
    task_json["id"] = task.id;
    task_json["title"] = title;
    task_json["description"] = description;
    task_json["status"] = "todo";
    task_json["project_id"] = project_id;
    task_json["creator_id"] = creator_id;
    task_json["assignee_id"] = assignee_id;
    task_json["tags"] = json::array();
    task_json["created_at"] = task.created_at;
    task_json["due_date"] = due_date;
    task_json["completed_at"] = 0;
    
    data["tasks"].push_back(task_json);
    saveDB();
    
    // Создаем уведомления
    createNotification(creator_id, "task_created", 
                      "Вы создали задачу '" + title + "'", task.id);
    
    if (!assignee_id.empty() && assignee_id != creator_id) {
        createNotification(assignee_id, "task_assigned", 
                          "Вам назначена задача '" + title + "'", task.id);
    }
    
    cout << "[TaskDB] Created task: " << title << " (ID: " << task.id << ")" << endl;
    return task;
}

Task TaskDB::getTask(const string& task_id) {
    for (const auto& task : data["tasks"]) {
        if (task["id"] == task_id) {
            Task t;
            t.id = task["id"].get<string>();
            t.title = task["title"].get<string>();
            t.description = task["description"].get<string>();
            t.status = task["status"].get<string>();
            t.project_id = task["project_id"].get<string>();
            t.creator_id = task["creator_id"].get<string>();
            t.assignee_id = task["assignee_id"].get<string>();
            t.created_at = task["created_at"].get<time_t>();
            t.due_date = task["due_date"].get<time_t>();
            t.completed_at = task["completed_at"].get<time_t>();
            
            for (const auto& tag : task["tags"]) {
                t.tags.push_back(tag.get<string>());
            }
            
            return t;
        }
    }
    return Task{};
}

vector<Task> TaskDB::getProjectTasks(const string& project_id) {
    vector<Task> tasks;
    
    for (const auto& task : data["tasks"]) {
        if (task["project_id"] == project_id) {
            Task t;
            t.id = task["id"].get<string>();
            t.title = task["title"].get<string>();
            t.description = task["description"].get<string>();
            t.status = task["status"].get<string>();
            t.project_id = task["project_id"].get<string>();
            t.creator_id = task["creator_id"].get<string>();
            t.assignee_id = task["assignee_id"].get<string>();
            t.created_at = task["created_at"].get<time_t>();
            t.due_date = task["due_date"].get<time_t>();
            t.completed_at = task["completed_at"].get<time_t>();
            
            for (const auto& tag : task["tags"]) {
                t.tags.push_back(tag.get<string>());
            }
            
            tasks.push_back(t);
        }
    }
    
    return tasks;
}

vector<Task> TaskDB::getUserTasks(const string& user_id) {
    vector<Task> tasks;
    
    for (const auto& task : data["tasks"]) {
        if (task["creator_id"] == user_id || task["assignee_id"] == user_id) {
            Task t;
            t.id = task["id"].get<string>();
            t.title = task["title"].get<string>();
            t.description = task["description"].get<string>();
            t.status = task["status"].get<string>();
            t.project_id = task["project_id"].get<string>();
            t.creator_id = task["creator_id"].get<string>();
            t.assignee_id = task["assignee_id"].get<string>();
            t.created_at = task["created_at"].get<time_t>();
            t.due_date = task["due_date"].get<time_t>();
            t.completed_at = task["completed_at"].get<time_t>();
            
            for (const auto& tag : task["tags"]) {
                t.tags.push_back(tag.get<string>());
            }
            
            tasks.push_back(t);
        }
    }
    
    return tasks;
}

vector<Task> TaskDB::getUserAssignedTasks(const string& user_id) {
    vector<Task> tasks;
    
    for (const auto& task : data["tasks"]) {
        if (task["assignee_id"] == user_id) {
            Task t;
            t.id = task["id"].get<string>();
            t.title = task["title"].get<string>();
            t.description = task["description"].get<string>();
            t.status = task["status"].get<string>();
            t.project_id = task["project_id"].get<string>();
            t.creator_id = task["creator_id"].get<string>();
            t.assignee_id = task["assignee_id"].get<string>();
            t.created_at = task["created_at"].get<time_t>();
            t.due_date = task["due_date"].get<time_t>();
            t.completed_at = task["completed_at"].get<time_t>();
            
            for (const auto& tag : task["tags"]) {
                t.tags.push_back(tag.get<string>());
            }
            
            tasks.push_back(t);
        }
    }
    
    return tasks;
}

bool TaskDB::updateTask(const string& task_id, 
                       const map<string, string>& updates) {
    for (auto& task : data["tasks"]) {
        if (task["id"] == task_id) {
            bool status_changed = false;
            
            for (const auto& update : updates) {
                if (update.first == "title" || update.first == "description" || 
                    update.first == "status" || update.first == "assignee_id") {
                    
                    if (update.first == "status" && task["status"] != update.second) {
                        status_changed = true;
                        
                        // Если задача завершена, устанавливаем completed_at
                        if (update.second == "done") {
                            task["completed_at"] = getCurrentTime();
                        }
                    }
                    
                    task[update.first] = update.second;
                }
            }
            
            saveDB();
            
            // Создаем уведомление при изменении статуса
            if (status_changed) {
                createNotification(task["assignee_id"].get<string>(), "status_change",
                                  "Статус задачи '" + task["title"].get<string>() + "' изменен",
                                  task_id);
            }
            
            cout << "[TaskDB] Updated task: " << task_id << endl;
            return true;
        }
    }
    return false;
}

bool TaskDB::updateTaskStatus(const string& task_id, const string& status) {
    map<string, string> updates;
    updates["status"] = status;
    return updateTask(task_id, updates);
}

bool TaskDB::assignTask(const string& task_id, const string& assignee_id) {
    for (auto& task : data["tasks"]) {
        if (task["id"] == task_id) {
            string old_assignee = task["assignee_id"].get<string>();
            task["assignee_id"] = assignee_id;
            saveDB();
            
            // Создаем уведомление новому исполнителю
            if (!assignee_id.empty() && assignee_id != old_assignee) {
                createNotification(assignee_id, "task_assigned",
                                  "Вам назначена задача '" + task["title"].get<string>() + "'",
                                  task_id);
            }
            
            cout << "[TaskDB] Assigned task " << task_id << " to user " << assignee_id << endl;
            return true;
        }
    }
    return false;
}

bool TaskDB::deleteTask(const string& task_id) {
    for (size_t i = 0; i < data["tasks"].size(); ++i) {
        if (data["tasks"][i]["id"] == task_id) {
            data["tasks"].erase(data["tasks"].begin() + i);
            saveDB();
            
            // Удаляем связанные комментарии
            vector<size_t> comments_to_remove;
            for (size_t j = 0; j < data["comments"].size(); ++j) {
                if (data["comments"][j]["task_id"] == task_id) {
                    comments_to_remove.push_back(j);
                }
            }
            
            // Удаляем в обратном порядке
            for (auto it = comments_to_remove.rbegin(); it != comments_to_remove.rend(); ++it) {
                data["comments"].erase(data["comments"].begin() + *it);
            }
            
            cout << "[TaskDB] Deleted task: " << task_id << endl;
            return true;
        }
    }
    return false;
}

Comment TaskDB::addComment(const string& task_id, 
                          const string& user_id,
                          const string& content) {
    Comment comment;
    comment.id = generateId();
    comment.task_id = task_id;
    comment.user_id = user_id;
    comment.content = content;
    comment.created_at = getCurrentTime();
    
    json comment_json;
    comment_json["id"] = comment.id;
    comment_json["task_id"] = task_id;
    comment_json["user_id"] = user_id;
    comment_json["content"] = content;
    comment_json["created_at"] = comment.created_at;
    
    data["comments"].push_back(comment_json);
    saveDB();
    
    // Создаем уведомление для исполнителя задачи
    for (const auto& task : data["tasks"]) {
        if (task["id"] == task_id) {
            string assignee_id = task["assignee_id"].get<string>();
            if (!assignee_id.empty() && assignee_id != user_id) {
                createNotification(assignee_id, "comment", 
                                  "Новый комментарий к задаче '" + task["title"].get<string>() + "'",
                                  task_id);
            }
            break;
        }
    }
    
    cout << "[TaskDB] Added comment to task: " << task_id << endl;
    return comment;
}

vector<Comment> TaskDB::getTaskComments(const string& task_id) {
    vector<Comment> comments;
    
    for (const auto& comment : data["comments"]) {
        if (comment["task_id"] == task_id) {
            Comment c;
            c.id = comment["id"].get<string>();
            c.task_id = comment["task_id"].get<string>();
            c.user_id = comment["user_id"].get<string>();
            c.content = comment["content"].get<string>();
            c.created_at = comment["created_at"].get<time_t>();
            
            comments.push_back(c);
        }
    }
    
    // Сортируем по времени создания (новые сверху)
    sort(comments.begin(), comments.end(), 
        [](const Comment& a, const Comment& b) {
            return a.created_at > b.created_at;
        });
    
    return comments;
}

bool TaskDB::deleteComment(const string& comment_id) {
    for (size_t i = 0; i < data["comments"].size(); ++i) {
        if (data["comments"][i]["id"] == comment_id) {
            data["comments"].erase(data["comments"].begin() + i);
            saveDB();
            
            cout << "[TaskDB] Deleted comment: " << comment_id << endl;
            return true;
        }
    }
    return false;
}

Notification TaskDB::createNotification(const string& user_id,
                                       const string& type,
                                       const string& message,
                                       const string& entity_id) {
    Notification notification;
    notification.id = generateId();
    notification.user_id = user_id;
    notification.type = type;
    notification.message = message;
    notification.entity_id = entity_id;
    notification.read = false;
    notification.created_at = getCurrentTime();
    
    json notification_json;
    notification_json["id"] = notification.id;
    notification_json["user_id"] = user_id;
    notification_json["type"] = type;
    notification_json["message"] = message;
    notification_json["entity_id"] = entity_id;
    notification_json["read"] = false;
    notification_json["created_at"] = notification.created_at;
    
    data["notifications"].push_back(notification_json);
    saveDB();
    
    cout << "[TaskDB] Created notification for user: " << user_id << endl;
    return notification;
}

vector<Notification> TaskDB::getUserNotifications(const string& user_id, bool unread_only) {
    vector<Notification> notifications;
    
    for (const auto& notification : data["notifications"]) {
        if (notification["user_id"] == user_id) {
            if (unread_only && notification["read"].get<bool>()) {
                continue;
            }
            
            Notification n;
            n.id = notification["id"].get<string>();
            n.user_id = notification["user_id"].get<string>();
            n.type = notification["type"].get<string>();
            n.message = notification["message"].get<string>();
            n.entity_id = notification["entity_id"].get<string>();
            n.read = notification["read"].get<bool>();
            n.created_at = notification["created_at"].get<time_t>();
            
            notifications.push_back(n);
        }
    }
    
    // Сортируем по времени (новые сверху)
    sort(notifications.begin(), notifications.end(),
        [](const Notification& a, const Notification& b) {
            return a.created_at > b.created_at;
        });
    
    return notifications;
}

bool TaskDB::markNotificationRead(const string& notification_id) {
    for (auto& notification : data["notifications"]) {
        if (notification["id"] == notification_id) {
            notification["read"] = true;
            saveDB();
            return true;
        }
    }
    return false;
}

bool TaskDB::markAllNotificationsRead(const string& user_id) {
    bool updated = false;
    
    for (auto& notification : data["notifications"]) {
        if (notification["user_id"] == user_id && !notification["read"].get<bool>()) {
            notification["read"] = true;
            updated = true;
        }
    }
    
    if (updated) {
        saveDB();
    }
    
    return updated;
}

vector<Task> TaskDB::searchTasks(const string& project_id, 
                                const string& query,
                                const string& status,
                                const string& assignee_id) {
    vector<Task> results;
    
    for (const auto& task : data["tasks"]) {
        // Проверяем проект
        if (!project_id.empty() && task["project_id"] != project_id) {
            continue;
        }
        
        // Проверяем статус
        if (!status.empty() && task["status"] != status) {
            continue;
        }
        
        // Проверяем исполнителя
        if (!assignee_id.empty() && task["assignee_id"] != assignee_id) {
            continue;
        }
        
        // Проверяем поисковый запрос
        if (!query.empty()) {
            string title = task["title"].get<string>();
            string description = task["description"].get<string>();
            
            if (title.find(query) == string::npos && 
                description.find(query) == string::npos) {
                continue;
            }
        }
        
        Task t;
        t.id = task["id"].get<string>();
        t.title = task["title"].get<string>();
        t.description = task["description"].get<string>();
        t.status = task["status"].get<string>();
        t.project_id = task["project_id"].get<string>();
        t.creator_id = task["creator_id"].get<string>();
        t.assignee_id = task["assignee_id"].get<string>();
        t.created_at = task["created_at"].get<time_t>();
        t.due_date = task["due_date"].get<time_t>();
        t.completed_at = task["completed_at"].get<time_t>();
        
        for (const auto& tag : task["tags"]) {
            t.tags.push_back(tag.get<string>());
        }
        
        results.push_back(t);
    }
    
    return results;
}

map<string, int> TaskDB::getProjectStats(const string& project_id) {
    map<string, int> stats;
    stats["total"] = 0;
    stats["todo"] = 0;
    stats["in_progress"] = 0;
    stats["done"] = 0;
    stats["review"] = 0;
    
    for (const auto& task : data["tasks"]) {
        if (task["project_id"] == project_id) {
            stats["total"]++;
            string status = task["status"].get<string>();
            
            if (status == "todo") stats["todo"]++;
            else if (status == "in_progress") stats["in_progress"]++;
            else if (status == "done") stats["done"]++;
            else if (status == "review") stats["review"]++;
        }
    }
    
    return stats;
}

map<string, int> TaskDB::getUserStats(const string& user_id) {
    map<string, int> stats;
    stats["assigned"] = 0;
    stats["completed"] = 0;
    stats["created"] = 0;
    
    for (const auto& task : data["tasks"]) {
        if (task["assignee_id"] == user_id) {
            stats["assigned"]++;
            if (task["status"] == "done") {
                stats["completed"]++;
            }
        }
        
        if (task["creator_id"] == user_id) {
            stats["created"]++;
        }
    }
    
    return stats;
}

bool TaskDB::addTaskTag(const string& task_id, const string& tag) {
    for (auto& task : data["tasks"]) {
        if (task["id"] == task_id) {
            // Проверяем, есть ли уже такой тег
            for (const auto& existing_tag : task["tags"]) {
                if (existing_tag == tag) {
                    return true; // Тег уже существует
                }
            }
            
            task["tags"].push_back(tag);
            saveDB();
            
            cout << "[TaskDB] Added tag '" << tag << "' to task: " << task_id << endl;
            return true;
        }
    }
    return false;
}

bool TaskDB::removeTaskTag(const string& task_id, const string& tag) {
    for (auto& task : data["tasks"]) {
        if (task["id"] == task_id) {
            json new_tags = json::array();
            bool found = false;
            
            for (const auto& existing_tag : task["tags"]) {
                if (existing_tag != tag) {
                    new_tags.push_back(existing_tag);
                } else {
                    found = true;
                }
            }
            
            if (found) {
                task["tags"] = new_tags;
                saveDB();
                
                cout << "[TaskDB] Removed tag '" << tag << "' from task: " << task_id << endl;
                return true;
            }
            break;
        }
    }
    return false;
}

vector<string> TaskDB::getTaskTags(const string& task_id) {
    vector<string> tags;
    
    for (const auto& task : data["tasks"]) {
        if (task["id"] == task_id) {
            for (const auto& tag : task["tags"]) {
                tags.push_back(tag.get<string>());
            }
            break;
        }
    }
    
    return tags;
}

vector<Task> TaskDB::getTasksByTag(const string& project_id, const string& tag) {
    vector<Task> tasks;
    
    for (const auto& task : data["tasks"]) {
        if (task["project_id"] == project_id) {
            bool has_tag = false;
            
            for (const auto& task_tag : task["tags"]) {
                if (task_tag == tag) {
                    has_tag = true;
                    break;
                }
            }
            
            if (has_tag) {
                Task t;
                t.id = task["id"].get<string>();
                t.title = task["title"].get<string>();
                t.description = task["description"].get<string>();
                t.status = task["status"].get<string>();
                t.project_id = task["project_id"].get<string>();
                t.creator_id = task["creator_id"].get<string>();
                t.assignee_id = task["assignee_id"].get<string>();
                t.created_at = task["created_at"].get<time_t>();
                t.due_date = task["due_date"].get<time_t>();
                t.completed_at = task["completed_at"].get<time_t>();
                
                for (const auto& task_tag : task["tags"]) {
                    t.tags.push_back(task_tag.get<string>());
                }
                
                tasks.push_back(t);
            }
        }
    }
    
    return tasks;
}